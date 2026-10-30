/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_unified_sr.h"
#include <assert.h>
#include <math.h>
#include <string.h>

int main(void)
{
    /* 1. Test upscaler and tier names */
    assert(strcmp(ffxVkUnifiedSrGetUpscalerName(FFX_VK_UPSCALER_AUTO), "Automatic (hardware recommendation)") == 0);
    assert(strcmp(ffxVkUnifiedSrGetUpscalerName(FFX_VK_UPSCALER_FSR3), "AMD FidelityFX Super Resolution 3.1") == 0);
    assert(strcmp(ffxVkUnifiedSrGetUpscalerName(FFX_VK_UPSCALER_FSR4), "AMD FidelityFX Super Resolution 4 (INT8/DOT4)") == 0);
    assert(strcmp(ffxVkUnifiedSrGetUpscalerName(FFX_VK_UPSCALER_DLSS), "DLSS spatial prototype") == 0);
    assert(strcmp(ffxVkUnifiedSrGetUpscalerName(FFX_VK_UPSCALER_XESS), "XeSS spatial prototype") == 0);

    assert(strstr(ffxVkUnifiedSrGetGpuTierName(FFX_VK_GPU_TIER_GENERIC_COMPUTE), "Generic Compute") != NULL);
    assert(strstr(ffxVkUnifiedSrGetGpuTierName(FFX_VK_GPU_TIER_INT8_DOT4), "Integer Dot Product") != NULL);
    assert(strstr(ffxVkUnifiedSrGetGpuTierName(FFX_VK_GPU_TIER_TENSOR_WMMA), "AMD Wave Matrix") != NULL);
    assert(strstr(ffxVkUnifiedSrGetGpuTierName(FFX_VK_GPU_TIER_TENSOR_CORES), "NVIDIA Tensor Cores") != NULL);

    /* 2. Test resolution scaling */
    VkExtent2D display = { 2560, 1440 };
    VkExtent2D render = { 0, 0 };

    assert(ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_NATIVE, display, &render));
    assert(render.width == 2560 && render.height == 1440);

    assert(ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_QUALITY, display, &render));
    assert(render.width == 1708 && render.height == 960); /* Even */

    assert(ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_PERFORMANCE, display, &render));
    assert(render.width == 1280 && render.height == 720);

    assert(ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_ULTRA_PERFORMANCE, display, &render));
    assert(render.width == 854 && render.height == 480);

    /* DRS preset returns valid even baseline resolution */
    assert(ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_DRS, display, &render));
    assert(render.width > 0 && render.height > 0);
    assert((render.width & 1u) == 0 && (render.height & 1u) == 0);

    /* Test DRS governor scaling */
    /* Underperforming: 25.0ms vs 16.6ms target -> scale decreases */
    float s1 = ffxVkUnifiedSrCalculateDrsScale(25.0f, 16.6f, 100.0f, 50.0f, 100.0f);
    assert(s1 < 100.0f);
    assert(s1 >= 50.0f);

    /* Overperforming: 10.0ms vs 16.6ms target -> scale increases */
    float s2 = ffxVkUnifiedSrCalculateDrsScale(10.0f, 16.6f, 60.0f, 50.0f, 100.0f);
    assert(s2 > 60.0f);
    assert(s2 <= 100.0f);

    /* Deadband margin: +-1% variation returns current scale */
    float s3 = ffxVkUnifiedSrCalculateDrsScale(16.5f, 16.6f, 75.0f, 50.0f, 100.0f);
    assert(fabsf(s3 - 75.0f) < 0.001f);

    /* Boundary clamp checks */
    float sMin = ffxVkUnifiedSrCalculateDrsScale(100.0f, 16.6f, 51.0f, 50.0f, 100.0f);
    assert(sMin >= 50.0f);
    float sMax = ffxVkUnifiedSrCalculateDrsScale(1.0f, 16.6f, 99.0f, 50.0f, 100.0f);
    assert(sMax <= 100.0f);

    /* 3. Test resolution auto-selection and fallback logic */
    FfxVkGpuCapabilities rtxCaps;
    memset(&rtxCaps, 0, sizeof(rtxCaps));
    rtxCaps.vendorId = FFX_VK_VENDOR_NVIDIA;
    rtxCaps.tier = FFX_VK_GPU_TIER_TENSOR_CORES;
    rtxCaps.supportsTensorCores = true;
    rtxCaps.supportsDot4 = true;
    rtxCaps.supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3) |
                                       (1u << FFX_VK_UPSCALER_FSR4) |
                                       (1u << FFX_VK_UPSCALER_XESS) |
                                       (1u << FFX_VK_UPSCALER_DLSS);
    rtxCaps.recommendedUpscaler = FFX_VK_UPSCALER_DLSS;

    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_AUTO, &rtxCaps) == FFX_VK_UPSCALER_DLSS);
    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_FSR4, &rtxCaps) == FFX_VK_UPSCALER_FSR4);

    /* AMD RDNA3 with WMMA */
    FfxVkGpuCapabilities rdna3Caps;
    memset(&rdna3Caps, 0, sizeof(rdna3Caps));
    rdna3Caps.vendorId = FFX_VK_VENDOR_AMD;
    rdna3Caps.tier = FFX_VK_GPU_TIER_TENSOR_WMMA;
    rdna3Caps.supportsWmma = true;
    rdna3Caps.supportsDot4 = true;
    rdna3Caps.supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3) |
                                         (1u << FFX_VK_UPSCALER_FSR4) |
                                         (1u << FFX_VK_UPSCALER_XESS) |
                                         (1u << FFX_VK_UPSCALER_DLSS); /* d4r supported */
    rdna3Caps.recommendedUpscaler = FFX_VK_UPSCALER_DLSS;

    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_AUTO, &rdna3Caps) == FFX_VK_UPSCALER_DLSS);

    /* AMD RDNA2 (Navi 22 / RX 6800M) */
    FfxVkGpuCapabilities rdna2Caps;
    memset(&rdna2Caps, 0, sizeof(rdna2Caps));
    rdna2Caps.vendorId = FFX_VK_VENDOR_AMD;
    rdna2Caps.tier = FFX_VK_GPU_TIER_INT8_DOT4;
    rdna2Caps.supportsDot4 = true;
    rdna2Caps.supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3) |
                                         (1u << FFX_VK_UPSCALER_FSR4) |
                                         (1u << FFX_VK_UPSCALER_XESS);
    rdna2Caps.recommendedUpscaler = FFX_VK_UPSCALER_FSR4;

    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_AUTO, &rdna2Caps) == FFX_VK_UPSCALER_FSR4);
    /* Requesting DLSS on RDNA2 gracefully falls back to FSR4 */
    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_DLSS, &rdna2Caps) == FFX_VK_UPSCALER_FSR4);

    /* Legacy GPU (e.g. Polaris / Pascal) without DOT4 */
    FfxVkGpuCapabilities legacyCaps;
    memset(&legacyCaps, 0, sizeof(legacyCaps));
    legacyCaps.tier = FFX_VK_GPU_TIER_GENERIC_COMPUTE;
    legacyCaps.supportedUpscalersBitmask = (1u << FFX_VK_UPSCALER_FSR3);
    legacyCaps.recommendedUpscaler = FFX_VK_UPSCALER_FSR3;

    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_AUTO, &legacyCaps) == FFX_VK_UPSCALER_FSR3);
    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_DLSS, &legacyCaps) == FFX_VK_UPSCALER_FSR3);
    assert(ffxVkUnifiedSrResolveUpscaler(FFX_VK_UPSCALER_FSR4, &legacyCaps) == FFX_VK_UPSCALER_FSR3);

    /* 4. Test Unified Context Lifecycle */
    FfxVkUnifiedSrContext ctx;
    FfxVkUnifiedSrCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.structSize = sizeof(FfxVkUnifiedSrCreateInfo);
    createInfo.version = FFX_VK_UNIFIED_SR_VERSION;
    createInfo.preferredUpscaler = FFX_VK_UPSCALER_AUTO;
    createInfo.quality = FFX_VK_QUALITY_PRESET_QUALITY;
    createInfo.maxInputExtent = (VkExtent2D){ 1708, 960 };
    createInfo.maxOutputExtent = (VkExtent2D){ 2560, 1440 };

    assert(ffxVkUnifiedSrCreate(&ctx, &createInfo) == VK_SUCCESS);
    assert(ctx.initialized == 1);

    FfxVkUnifiedSrDispatchInfo dispatchInfo;
    memset(&dispatchInfo, 0, sizeof(dispatchInfo));
    dispatchInfo.structSize = sizeof(FfxVkUnifiedSrDispatchInfo);
    dispatchInfo.version = FFX_VK_UNIFIED_SR_VERSION;
    dispatchInfo.commandBuffer = (VkCommandBuffer)(uintptr_t)1;
    dispatchInfo.inputExtent = (VkExtent2D){ 1708, 960 };
    dispatchInfo.outputExtent = (VkExtent2D){ 2560, 1440 };

    const char *reason = NULL;
    assert(ffxVkUnifiedSrGetReadiness(NULL, &reason) == VK_ERROR_INITIALIZATION_FAILED);
    assert(reason && reason[0]);
    assert(ffxVkUnifiedSrGetReadiness(&ctx, &reason) == VK_ERROR_FEATURE_NOT_PRESENT);
    assert(strstr(reason, "not implemented"));
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_FEATURE_NOT_PRESENT);
    ctx.activeUpscaler = FFX_VK_UPSCALER_FSR4;
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_FEATURE_NOT_PRESENT);
    ctx.activeUpscaler = FFX_VK_UPSCALER_XESS;
    assert(ffxVkUnifiedSrGetReadiness(&ctx, NULL) == VK_ERROR_INITIALIZATION_FAILED);
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_INITIALIZATION_FAILED);
    ctx.activeUpscaler = FFX_VK_UPSCALER_DLSS;
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_INITIALIZATION_FAILED);

    /* Fake handles exercise rejection before any Vulkan call. A pipeline
     * must not let missing resources turn dispatch into a successful no-op. */
    ctx.createInfo.device = (VkDevice)(uintptr_t)1;
    ctx.dlssPipeline.pipeline = (VkPipeline)(uintptr_t)1;
    assert(ffxVkUnifiedSrGetReadiness(&ctx, NULL) == VK_SUCCESS);
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_VALIDATION_FAILED_EXT);
    ctx.activeUpscaler = FFX_VK_UPSCALER_XESS;
    ctx.xessPipeline.pipeline = (VkPipeline)(uintptr_t)1;
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_VALIDATION_FAILED_EXT);
    ctx.activeUpscaler = FFX_VK_UPSCALER_COUNT;
    assert(ffxVkUnifiedSrDispatch(&ctx, &dispatchInfo) == VK_ERROR_FEATURE_NOT_PRESENT);
    ctx.createInfo.device = VK_NULL_HANDLE;
    ctx.dlssPipeline.pipeline = VK_NULL_HANDLE;
    ctx.xessPipeline.pipeline = VK_NULL_HANDLE;

    ffxVkUnifiedSrDestroy(&ctx);
    assert(ctx.initialized == 0);
    assert(ffxVkUnifiedSrGetReadiness(&ctx, NULL) == VK_ERROR_INITIALIZATION_FAILED);
    createInfo.preferredUpscaler = FFX_VK_UPSCALER_COUNT;
    assert(ffxVkUnifiedSrCreate(&ctx, &createInfo) == VK_ERROR_INITIALIZATION_FAILED);

    return 0;
}
