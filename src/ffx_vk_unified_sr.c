/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_unified_sr.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *ffxVkUnifiedSrGetUpscalerName(FfxVkUpscalerType upscaler)
{
    switch (upscaler) {
    case FFX_VK_UPSCALER_AUTO:
        return "Automatic (hardware recommendation)";
    case FFX_VK_UPSCALER_FSR3:
        return "AMD FidelityFX Super Resolution 3.1";
    case FFX_VK_UPSCALER_FSR4:
        return "AMD FidelityFX Super Resolution 4 (INT8/DOT4)";
    case FFX_VK_UPSCALER_DLSS:
        return "DLSS spatial prototype";
    case FFX_VK_UPSCALER_XESS:
        return "XeSS spatial prototype";
    default:
        return "Unknown Upscaler";
    }
}

const char *ffxVkUnifiedSrGetGpuTierName(FfxVkGpuTier tier)
{
    switch (tier) {
    case FFX_VK_GPU_TIER_GENERIC_COMPUTE:
        return "Tier 1: Generic Compute (All GPUs)";
    case FFX_VK_GPU_TIER_INT8_DOT4:
        return "Tier 2: Integer Dot Product / DP4a";
    case FFX_VK_GPU_TIER_TENSOR_WMMA:
        return "Tier 3: AMD Wave Matrix (RDNA3/RDNA4 WMMA/FP8)";
    case FFX_VK_GPU_TIER_TENSOR_CORES:
        return "Tier 3: NVIDIA Tensor Cores (RTX 20/30/40/50)";
    default:
        return "Unknown Tier";
    }
}

bool ffxVkUnifiedSrGetRenderResolution(
    FfxVkQualityPreset quality,
    VkExtent2D outputExtent,
    VkExtent2D *outInputExtent)
{
    if (!outInputExtent || !outputExtent.width || !outputExtent.height)
        return false;

    float scale = 1.5f; /* Default Quality */
    switch (quality) {
    case FFX_VK_QUALITY_PRESET_NATIVE:
        scale = 1.0f;
        break;
    case FFX_VK_QUALITY_PRESET_QUALITY:
        scale = 1.5f;
        break;
    case FFX_VK_QUALITY_PRESET_BALANCED:
        scale = 1.7f;
        break;
    case FFX_VK_QUALITY_PRESET_PERFORMANCE:
        scale = 2.0f;
        break;
    case FFX_VK_QUALITY_PRESET_ULTRA_PERFORMANCE:
        scale = 3.0f;
        break;
    case FFX_VK_QUALITY_PRESET_DRS:
        scale = 1.5f; /* Dynamic resolution scaling baseline (Quality scale) */
        break;
    default:
        return false;
    }

    outInputExtent->width = (uint32_t)ceilf((float)outputExtent.width / scale);
    outInputExtent->height = (uint32_t)ceilf((float)outputExtent.height / scale);

    /* Guarantee even extents for downsampled/upsampled convolutional pipelines */
    if (outInputExtent->width & 1u)
        outInputExtent->width += 1u;
    if (outInputExtent->height & 1u)
        outInputExtent->height += 1u;

    return true;
}

float ffxVkUnifiedSrCalculateDrsScale(
    float currentFrameTimeMs,
    float targetFrameTimeMs,
    float currentScale,
    float minScale,
    float maxScale)
{
    if (targetFrameTimeMs <= 0.0f || currentFrameTimeMs <= 0.0f)
        return currentScale;

    if (minScale > maxScale) {
        float tmp = minScale;
        minScale = maxScale;
        maxScale = tmp;
    }

    /* Target error: positive when running faster than target, negative when overloaded */
    float error = 1.0f - (currentFrameTimeMs / targetFrameTimeMs);

    /* Deadband: +-2% jitter margin */
    if (fabsf(error) < 0.02f) {
        if (currentScale < minScale) return minScale;
        if (currentScale > maxScale) return maxScale;
        return currentScale;
    }

    /* Adaptive adjustment step with damping to prevent oscillation */
    float step = error * 0.10f * currentScale;

    /* Asymmetric damping: downscale faster (1.25x) to recover framerate quickly */
    if (step < 0.0f)
        step *= 1.25f;

    float newScale = currentScale + step;
    if (newScale < minScale)
        newScale = minScale;
    if (newScale > maxScale)
        newScale = maxScale;

    return newScale;
}

bool ffxVkUnifiedSrQueryGpuCapabilities(
    VkPhysicalDevice physicalDevice,
    FfxVkGpuCapabilities *outCaps)
{
    if (!physicalDevice || !outCaps)
        return false;

    memset(outCaps, 0, sizeof(*outCaps));

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);

    outCaps->vendorId = props.vendorID;
    outCaps->deviceId = props.deviceID;
    snprintf(outCaps->deviceName, sizeof(outCaps->deviceName), "%s", props.deviceName);

    switch (props.vendorID) {
    case FFX_VK_VENDOR_AMD:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "AMD");
        break;
    case FFX_VK_VENDOR_NVIDIA:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "NVIDIA");
        break;
    case FFX_VK_VENDOR_INTEL:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "Intel");
        break;
    case FFX_VK_VENDOR_ARM:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "ARM");
        break;
    case FFX_VK_VENDOR_QUALCOMM:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "Qualcomm");
        break;
    default:
        snprintf(outCaps->vendorName, sizeof(outCaps->vendorName), "Unknown");
        break;
    }

    /* Probe features */
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(physicalDevice, &features);

    /* Baseline compute capability */
    outCaps->supportsFp16 = true;
    outCaps->supportsFormatlessWrite = features.shaderStorageImageWriteWithoutFormat;

    /* Vendor and architecture heuristics */
    if (outCaps->vendorId == FFX_VK_VENDOR_NVIDIA) {
        /* Check for RTX (Turing, Ampere, Ada Lovelace, Blackwell) */
        if (strstr(outCaps->deviceName, "RTX") || strstr(outCaps->deviceName, "rtx") ||
            strstr(outCaps->deviceName, "TITAN V") || strstr(outCaps->deviceName, "Quadro RTX")) {
            outCaps->supportsTensorCores = true;
            outCaps->supportsInt8 = true;
            outCaps->supportsDot4 = true;
            outCaps->supportsBufferDeviceAddress = true;
            outCaps->tier = FFX_VK_GPU_TIER_TENSOR_CORES;
        } else {
            outCaps->tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
        }
    } else if (outCaps->vendorId == FFX_VK_VENDOR_AMD) {
        /* Check for RDNA3 (Navi 3x, gfx110x, RX 7xxx) and RDNA4 (Navi 4x, gfx120x, RX 8xxx) */
        if (strstr(outCaps->deviceName, "RX 7") || strstr(outCaps->deviceName, "RX 8") ||
            strstr(outCaps->deviceName, "Navi 3") || strstr(outCaps->deviceName, "Navi 4") ||
            strstr(outCaps->deviceName, "NAVI3") || strstr(outCaps->deviceName, "NAVI4") ||
            strstr(outCaps->deviceName, "gfx11") || strstr(outCaps->deviceName, "gfx12") ||
            strstr(outCaps->deviceName, "Radeon 7") || strstr(outCaps->deviceName, "Radeon 8")) {
            outCaps->supportsWmma = true;
            outCaps->supportsInt8 = true;
            outCaps->supportsDot4 = true;
            outCaps->supportsBufferDeviceAddress = true;
            outCaps->tier = FFX_VK_GPU_TIER_TENSOR_WMMA;
        } else if (strstr(outCaps->deviceName, "RX 6") || strstr(outCaps->deviceName, "Navi 2") ||
                   strstr(outCaps->deviceName, "NAVI2") || strstr(outCaps->deviceName, "gfx103")) {
            /* RDNA2 (Navi 2x, RX 6xxx) supports INT8/DOT4 but lacks dedicated WMMA */
            outCaps->supportsInt8 = true;
            outCaps->supportsDot4 = true;
            outCaps->supportsBufferDeviceAddress = true;
            outCaps->tier = FFX_VK_GPU_TIER_INT8_DOT4;
        } else {
            outCaps->tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
        }
    } else if (outCaps->vendorId == FFX_VK_VENDOR_INTEL) {
        if (strstr(outCaps->deviceName, "Arc") || strstr(outCaps->deviceName, "arc") ||
            strstr(outCaps->deviceName, "Alchemist") || strstr(outCaps->deviceName, "Battlemage") ||
            strstr(outCaps->deviceName, "Xe")) {
            outCaps->supportsInt8 = true;
            outCaps->supportsDot4 = true;
            outCaps->supportsBufferDeviceAddress = true;
            outCaps->tier = FFX_VK_GPU_TIER_INT8_DOT4;
        } else {
            outCaps->tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
        }
    } else {
        outCaps->tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
    }

    /* Bitmask of supported upscalers */
    outCaps->supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3); /* FSR 3 is universally supported */

    if (outCaps->supportsDot4 || outCaps->tier >= FFX_VK_GPU_TIER_INT8_DOT4) {
        outCaps->supportedUpscalersBitmask |= (1u << FFX_VK_UPSCALER_FSR4);
        outCaps->supportedUpscalersBitmask |= (1u << FFX_VK_UPSCALER_XESS); /* XeSS DP4a path */
    }

    if (outCaps->supportsTensorCores || outCaps->supportsWmma) {
        outCaps->supportedUpscalersBitmask |= (1u << FFX_VK_UPSCALER_DLSS); /* NVNGX or d4r */
    }

    /* Recommended upscaler choice */
    if (outCaps->supportsTensorCores) {
        outCaps->recommendedUpscaler = FFX_VK_UPSCALER_DLSS;
    } else if (outCaps->supportsWmma) {
        outCaps->recommendedUpscaler = FFX_VK_UPSCALER_DLSS; /* d4r on RDNA3/RDNA4 */
    } else if (outCaps->tier == FFX_VK_GPU_TIER_INT8_DOT4) {
        outCaps->recommendedUpscaler = FFX_VK_UPSCALER_FSR4; /* FSR4 INT8 on RDNA2 / Intel Arc */
    } else {
        outCaps->recommendedUpscaler = FFX_VK_UPSCALER_FSR3; /* FSR 3.1 on legacy GPUs */
    }

    return true;
}

FfxVkUpscalerType ffxVkUnifiedSrResolveUpscaler(
    FfxVkUpscalerType preferred,
    const FfxVkGpuCapabilities *caps)
{
    if (!caps)
        return FFX_VK_UPSCALER_FSR3;

    if (preferred == FFX_VK_UPSCALER_AUTO)
        return caps->recommendedUpscaler;

    /* If preferred upscaler is supported on this GPU, honor it */
    if (caps->supportedUpscalersBitmask & (1u << preferred))
        return preferred;

    /* Fallback chain: DLSS -> FSR4 -> XeSS -> FSR3 */
    if (preferred == FFX_VK_UPSCALER_DLSS) {
        if (caps->supportedUpscalersBitmask & (1u << FFX_VK_UPSCALER_FSR4))
            return FFX_VK_UPSCALER_FSR4;
        if (caps->supportedUpscalersBitmask & (1u << FFX_VK_UPSCALER_XESS))
            return FFX_VK_UPSCALER_XESS;
    } else if (preferred == FFX_VK_UPSCALER_FSR4 || preferred == FFX_VK_UPSCALER_XESS) {
        if (caps->supportedUpscalersBitmask & (1u << FFX_VK_UPSCALER_FSR4))
            return FFX_VK_UPSCALER_FSR4;
    }

    return FFX_VK_UPSCALER_FSR3;
}

VkResult ffxVkUnifiedSrCreate(
    FfxVkUnifiedSrContext *ctx,
    const FfxVkUnifiedSrCreateInfo *info)
{
    if (!ctx || !info)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (info->structSize != sizeof(FfxVkUnifiedSrCreateInfo) ||
        info->version != FFX_VK_UNIFIED_SR_VERSION ||
        info->preferredUpscaler < FFX_VK_UPSCALER_AUTO ||
        info->preferredUpscaler >= FFX_VK_UPSCALER_COUNT)
        return VK_ERROR_INITIALIZATION_FAILED;

    memset(ctx, 0, sizeof(*ctx));
    ctx->createInfo = *info;

    /* Query GPU capabilities */
    if (!ffxVkUnifiedSrQueryGpuCapabilities(info->physicalDevice, &ctx->capabilities)) {
        /* Fallback synthetic capabilities if physical device properties query fails */
        ctx->capabilities.tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
        ctx->capabilities.supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3);
        ctx->capabilities.recommendedUpscaler = FFX_VK_UPSCALER_FSR3;
    }

    ctx->activeUpscaler = ffxVkUnifiedSrResolveUpscaler(info->preferredUpscaler, &ctx->capabilities);
    ctx->initialized = 1;

    /* Initialize native compute pipelines if a valid Vulkan device was provided */
    if (info->device != VK_NULL_HANDLE) {
        FfxVkPortableResult result = FFX_VK_PORTABLE_OK;
        if (ctx->activeUpscaler == FFX_VK_UPSCALER_XESS) {
            result = ffxVkXessCreatePipeline(info->device, &ctx->xessPipeline);
        } else if (ctx->activeUpscaler == FFX_VK_UPSCALER_DLSS) {
            result = ffxVkDlssCreatePipeline(info->device, &ctx->dlssPipeline);
        } else if ((ctx->activeUpscaler == FFX_VK_UPSCALER_FSR3 ||
                    ctx->activeUpscaler == FFX_VK_UPSCALER_FSR4) &&
                   info->physicalDevice != VK_NULL_HANDLE &&
                   info->maxInputExtent.width > 0 && info->maxInputExtent.height > 0 &&
                   info->maxOutputExtent.width > 0 && info->maxOutputExtent.height > 0) {
            FfxVkPortableDeviceInfo devInfo;
            FfxVkPortableUpscaleCreateInfo upInfo;
            FfxVkPortableUpscaleContext *fsr3Ctx = NULL;
            memset(&devInfo, 0, sizeof(devInfo));
            devInfo.structSize = sizeof(devInfo);
            devInfo.physicalDevice = info->physicalDevice;
            devInfo.device = info->device;
            devInfo.getDeviceProcAddr = vkGetDeviceProcAddr;
            devInfo.shaderFloat16Enabled = ctx->capabilities.supportsFp16 ? VK_TRUE : VK_FALSE;
            devInfo.shaderStorageImageWriteWithoutFormatEnabled =
                ctx->capabilities.supportsFormatlessWrite ? VK_TRUE : VK_FALSE;

            memset(&upInfo, 0, sizeof(upInfo));
            upInfo.structSize = sizeof(upInfo);
            upInfo.flags = FFX_VK_PORTABLE_CONTEXT_HDR_COLOR_INPUT |
                FFX_VK_PORTABLE_CONTEXT_AUTO_EXPOSURE;
            upInfo.maxRenderSize.width = info->maxInputExtent.width;
            upInfo.maxRenderSize.height = info->maxInputExtent.height;
            upInfo.maxOutputSize.width = info->maxOutputExtent.width;
            upInfo.maxOutputSize.height = info->maxOutputExtent.height;

            if (ffxVkPortableUpscaleContextCreate(&devInfo, &upInfo, &fsr3Ctx) == FFX_VK_PORTABLE_OK) {
                ctx->backendContext = fsr3Ctx;
            }
        }
        if (result != FFX_VK_PORTABLE_OK) {
            ffxVkUnifiedSrDestroy(ctx);
            return VK_ERROR_INITIALIZATION_FAILED;
        }
    }

    return VK_SUCCESS;
}

VkResult ffxVkUnifiedSrGetReadiness(
    const FfxVkUnifiedSrContext *ctx, const char **reason)
{
    VkResult result = VK_ERROR_INITIALIZATION_FAILED;
    const char *message = "Unified SR context is not initialized";
    if (ctx && ctx->initialized) {
        switch (ctx->activeUpscaler) {
        case FFX_VK_UPSCALER_FSR3:
            if (ctx->createInfo.device && ctx->backendContext) {
                result = VK_SUCCESS;
                message = "AMD FidelityFX Super Resolution 3.1.4 native Vulkan ready";
            } else {
                message = "Auto unavailable: FSR dispatch integration is not implemented";
                result = VK_ERROR_FEATURE_NOT_PRESENT;
            }
            break;
        case FFX_VK_UPSCALER_FSR4:
            if (ctx->createInfo.device && ctx->backendContext) {
                result = VK_SUCCESS;
                message = "AMD FidelityFX Super Resolution 4 / 3.1.4 native Vulkan ready";
            } else {
                message = "Auto unavailable: FSR dispatch integration is not implemented";
                result = VK_ERROR_FEATURE_NOT_PRESENT;
            }
            break;
        case FFX_VK_UPSCALER_XESS:
            message = "XeSS prototype pipeline is unavailable";
            if (ctx->createInfo.device && ctx->xessPipeline.pipeline) {
                result = VK_SUCCESS;
                message = "XeSS spatial prototype ready (not recovered model inference)";
            }
            break;
        case FFX_VK_UPSCALER_DLSS:
            message = "DLSS prototype pipeline is unavailable";
            if (ctx->createInfo.device && ctx->dlssPipeline.pipeline) {
                result = VK_SUCCESS;
                message = "DLSS spatial prototype ready (not recovered model inference)";
            }
            break;
        default:
            message = "Unified SR selected backend is invalid";
            result = VK_ERROR_FEATURE_NOT_PRESENT;
            break;
        }
    }
    if (reason)
        *reason = message;
    return result;
}

VkResult ffxVkUnifiedSrDispatch(
    FfxVkUnifiedSrContext *ctx,
    const FfxVkUnifiedSrDispatchInfo *dispatchInfo)
{
    if (!ctx || !ctx->initialized || !dispatchInfo)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (dispatchInfo->structSize != sizeof(FfxVkUnifiedSrDispatchInfo) ||
        dispatchInfo->version != FFX_VK_UNIFIED_SR_VERSION)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (!dispatchInfo->commandBuffer)
        return VK_ERROR_INITIALIZATION_FAILED;

    if (!dispatchInfo->inputExtent.width || !dispatchInfo->inputExtent.height ||
        !dispatchInfo->outputExtent.width || !dispatchInfo->outputExtent.height)
        return VK_ERROR_INITIALIZATION_FAILED;

    VkResult readiness = ffxVkUnifiedSrGetReadiness(ctx, NULL);
    if (readiness != VK_SUCCESS)
        return readiness;

    /* Missing images/views must never turn this into a successful no-op. */
    if (!dispatchInfo->colorIn.image || !dispatchInfo->colorIn.view ||
        !dispatchInfo->depth.image || !dispatchInfo->depth.view ||
        !dispatchInfo->motionVectors.image || !dispatchInfo->motionVectors.view ||
        !dispatchInfo->colorOut.image || !dispatchInfo->colorOut.view)
        return VK_ERROR_VALIDATION_FAILED_EXT;

    /* Route dispatch according to active upscaler */
    switch (ctx->activeUpscaler) {
    case FFX_VK_UPSCALER_XESS: {
        FfxVkXessCreateInfo xInfo;
        memset(&xInfo, 0, sizeof(xInfo));
        xInfo.structSize = sizeof(xInfo);
        xInfo.contractVersion = FFX_VK_XESS_CONTRACT_VERSION;
        xInfo.path = FFX_VK_XESS_PATH_DP4A;
        xInfo.maxInputExtent = ctx->createInfo.maxInputExtent;
        xInfo.maxOutputExtent = ctx->createInfo.maxOutputExtent;

        FfxVkXessDispatchInfo xDsp;
        memset(&xDsp, 0, sizeof(xDsp));
        xDsp.structSize = sizeof(xDsp);
        xDsp.contractVersion = FFX_VK_XESS_CONTRACT_VERSION;
        xDsp.commandBuffer = dispatchInfo->commandBuffer;
        xDsp.colorIn = dispatchInfo->colorIn;
        xDsp.velocity = dispatchInfo->motionVectors;
        xDsp.depth = dispatchInfo->depth;
        xDsp.colorOut = dispatchInfo->colorOut;
        xDsp.inputExtent = dispatchInfo->inputExtent;
        xDsp.outputExtent = dispatchInfo->outputExtent;
        xDsp.jitterOffsetX = dispatchInfo->jitterOffsetX;
        xDsp.jitterOffsetY = dispatchInfo->jitterOffsetY;
        xDsp.sharpness = dispatchInfo->sharpness;
        xDsp.resetHistory = dispatchInfo->resetHistory;

        if (dispatchInfo->colorIn.image != VK_NULL_HANDLE) {
            uint64_t issues = ffxVkXessValidateDispatchInfo(&xInfo, &xDsp);
            if (issues != FFX_VK_XESS_VALIDATION_NONE)
                return VK_ERROR_VALIDATION_FAILED_EXT;
        }

        if (ctx->xessPipeline.pipeline != VK_NULL_HANDLE && dispatchInfo->colorIn.view != VK_NULL_HANDLE) {
            FfxVkPortableResult pres = ffxVkXessExecuteDispatch(dispatchInfo->commandBuffer, &ctx->xessPipeline, &xDsp);
            if (pres != FFX_VK_PORTABLE_OK)
                return VK_ERROR_UNKNOWN;
        }
        break;
    }
    case FFX_VK_UPSCALER_DLSS: {
        FfxVkDlssCreateInfo dInfo;
        memset(&dInfo, 0, sizeof(dInfo));
        dInfo.structSize = sizeof(dInfo);
        dInfo.contractVersion = FFX_VK_DLSS_CONTRACT_VERSION;
        dInfo.model = FFX_VK_DLSS_MODEL_4_SWIN_K;
        dInfo.preset = FFX_VK_DLSS_PRESET_PERFORMANCE;
        dInfo.flags = FFX_VK_DLSS_FLAG_AUTO_EXPOSURE | FFX_VK_DLSS_FLAG_HDR_INPUT;
        dInfo.gpuArch = (ctx->capabilities.tier == FFX_VK_GPU_TIER_TENSOR_CORES)
            ? FFX_VK_DLSS_ARCH_NVIDIA_TENSOR
            : ((ctx->capabilities.tier == FFX_VK_GPU_TIER_TENSOR_WMMA)
                ? FFX_VK_DLSS_ARCH_RDNA3
                : FFX_VK_DLSS_ARCH_GENERIC_VULKAN);
        dInfo.maxRenderSize = (FfxVkPortableExtent2D){ ctx->createInfo.maxInputExtent.width, ctx->createInfo.maxInputExtent.height };
        dInfo.displaySize = (FfxVkPortableExtent2D){ ctx->createInfo.maxOutputExtent.width, ctx->createInfo.maxOutputExtent.height };

        FfxVkDlssDispatchInfo dDsp;
        memset(&dDsp, 0, sizeof(dDsp));
        dDsp.structSize = sizeof(dDsp);
        dDsp.contractVersion = FFX_VK_DLSS_CONTRACT_VERSION;
        dDsp.renderSize = (FfxVkPortableExtent2D){ dispatchInfo->inputExtent.width, dispatchInfo->inputExtent.height };
        dDsp.color = dispatchInfo->colorIn;
        dDsp.depth = dispatchInfo->depth;
        dDsp.motionVectors = dispatchInfo->motionVectors;
        dDsp.output = dispatchInfo->colorOut;
        dDsp.jitterOffset = (FfxVkPortableFloat2){ dispatchInfo->jitterOffsetX, dispatchInfo->jitterOffsetY };
        dDsp.motionVectorScale = (FfxVkPortableFloat2){
            (float)(dispatchInfo->inputExtent.width > 0 ? dispatchInfo->inputExtent.width : 1),
            (float)(dispatchInfo->inputExtent.height > 0 ? dispatchInfo->inputExtent.height : 1)
        };
        dDsp.verticalFov = 1.0f;
        dDsp.nearZ = 0.1f;
        dDsp.farZ = 1000.0f;
        dDsp.preExposure = 1.0f;
        dDsp.frameReset = dispatchInfo->resetHistory ? VK_TRUE : VK_FALSE;

        if (dispatchInfo->colorIn.image != VK_NULL_HANDLE) {
            uint64_t issues = 0;
            FfxVkPortableResult res = ffxVkDlssValidateDispatchInfo(&dInfo, &dDsp, &issues);
            if (res != FFX_VK_PORTABLE_OK || issues != 0)
                return VK_ERROR_VALIDATION_FAILED_EXT;
        }

        if (ctx->dlssPipeline.pipeline != VK_NULL_HANDLE && dispatchInfo->colorIn.view != VK_NULL_HANDLE) {
            FfxVkPortableResult pres = ffxVkDlssExecuteDispatch(dispatchInfo->commandBuffer, &ctx->dlssPipeline, &dDsp);
            if (pres != FFX_VK_PORTABLE_OK)
                return VK_ERROR_UNKNOWN;
        }
        break;
    }
    case FFX_VK_UPSCALER_FSR3:
    case FFX_VK_UPSCALER_FSR4: {
        if (!ctx->backendContext)
            return VK_ERROR_FEATURE_NOT_PRESENT;
        FfxVkPortableUpscaleDispatchInfo fDsp;
        memset(&fDsp, 0, sizeof(fDsp));
        fDsp.structSize = sizeof(fDsp);
        fDsp.commandBuffer = dispatchInfo->commandBuffer;
        fDsp.color = dispatchInfo->colorIn;
        fDsp.depth = dispatchInfo->depth;
        fDsp.motionVectors = dispatchInfo->motionVectors;
        fDsp.exposure = dispatchInfo->exposure;
        if (!fDsp.exposure.structSize)
            fDsp.exposure.structSize = sizeof(fDsp.exposure);
        fDsp.reactiveMask = dispatchInfo->reactiveMask;
        if (!fDsp.reactiveMask.structSize)
            fDsp.reactiveMask.structSize = sizeof(fDsp.reactiveMask);
        fDsp.transparencyAndCompositionMask.structSize =
            sizeof(fDsp.transparencyAndCompositionMask);
        fDsp.output = dispatchInfo->colorOut;
        fDsp.jitterOffset.x = dispatchInfo->jitterOffsetX;
        fDsp.jitterOffset.y = dispatchInfo->jitterOffsetY;
        fDsp.motionVectorScale.x = (float)dispatchInfo->inputExtent.width;
        fDsp.motionVectorScale.y = (float)dispatchInfo->inputExtent.height;
        fDsp.renderSize.width = dispatchInfo->inputExtent.width;
        fDsp.renderSize.height = dispatchInfo->inputExtent.height;
        fDsp.outputSize.width = dispatchInfo->outputExtent.width;
        fDsp.outputSize.height = dispatchInfo->outputExtent.height;
        fDsp.enableSharpening = dispatchInfo->sharpness > 0.0f ? VK_TRUE : VK_FALSE;
        fDsp.sharpness = dispatchInfo->sharpness;
        fDsp.frameTimeMilliseconds = dispatchInfo->frameTimeMs > 0.0f
            ? dispatchInfo->frameTimeMs : 16.667f;
        fDsp.preExposure = 1.0f;
        fDsp.reset = dispatchInfo->resetHistory ? VK_TRUE : VK_FALSE;
        fDsp.cameraNear = 0.1f;
        fDsp.cameraFar = 1000.0f;
        fDsp.cameraVerticalFovRadians = 1.0f;
        fDsp.viewSpaceToMeters = 1.0f;
        FfxVkPortableResult pres = ffxVkPortableUpscaleContextRecordDispatch(
            (FfxVkPortableUpscaleContext *)ctx->backendContext, &fDsp);
        if (pres != FFX_VK_PORTABLE_OK)
            return VK_ERROR_UNKNOWN;
        break;
    }
    default:
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    return VK_SUCCESS;
}

void ffxVkUnifiedSrDestroy(FfxVkUnifiedSrContext *ctx)
{
    if (!ctx || !ctx->initialized)
        return;

    if (ctx->createInfo.device != VK_NULL_HANDLE) {
        if (ctx->backendContext != NULL) {
            (void)ffxVkPortableUpscaleContextDestroy(
                (FfxVkPortableUpscaleContext *)ctx->backendContext);
        }
        ffxVkXessDestroyPipeline(ctx->createInfo.device, &ctx->xessPipeline);
        ffxVkDlssDestroyPipeline(ctx->createInfo.device, &ctx->dlssPipeline);
    }

    ctx->initialized = 0;
    ctx->backendContext = NULL;
}

