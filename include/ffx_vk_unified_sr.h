/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FFX_VK_UNIFIED_SR_H
#define FFX_VK_UNIFIED_SR_H

#include "ffx_vk_portable.h"
#include "ffx_vk_dlss_contract.h"
#include "ffx_vk_xess_contract.h"
#include "ffx_vk_fsr4_v07.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Single, unified Super Resolution API for any Vulkan game engine.
 * Automatically queries GPU hardware vendor (AMD, NVIDIA, Intel), detects
 * hardware feature tiers (Generic Compute, DP4a/INT8, AMD WMMA, NVIDIA Tensor Cores),
 * auto-selects the optimal upscaler, and provides a 1-line integration path. */
#define FFX_VK_UNIFIED_SR_VERSION 1u

/* Upscaler offerings */
typedef enum FfxVkUpscalerType {
    FFX_VK_UPSCALER_AUTO = 0,               /* Auto-detect based on GPU vendor and feature tier */
    FFX_VK_UPSCALER_FSR3 = 1,               /* AMD FidelityFX Super Resolution 3.1.4 / 3.1.5 (generic compute) */
    FFX_VK_UPSCALER_FSR4 = 2,               /* AMD FSR 4 v07 neural upscaler (INT8/DOT4) */
    FFX_VK_UPSCALER_DLSS = 3,               /* NVIDIA DLSS (Tensor Cores) or d4r (AMD RDNA3/4 WMMA) */
    FFX_VK_UPSCALER_XESS = 4,               /* Intel XeSS (DP4a cross-vendor or XMX on Arc) */
    FFX_VK_UPSCALER_COUNT = 5
} FfxVkUpscalerType;

/* Standard quality scaling presets */
typedef enum FfxVkQualityPreset {
    FFX_VK_QUALITY_PRESET_NATIVE = 0,           /* 1.0x (Native resolution / DLAA) */
    FFX_VK_QUALITY_PRESET_QUALITY = 1,          /* ~1.5x scale (67% input render scale) */
    FFX_VK_QUALITY_PRESET_BALANCED = 2,         /* ~1.7x scale (59% input render scale) */
    FFX_VK_QUALITY_PRESET_PERFORMANCE = 3,      /* ~2.0x scale (50% input render scale) */
    FFX_VK_QUALITY_PRESET_ULTRA_PERFORMANCE = 4, /* ~3.0x scale (33% input render scale) */
    FFX_VK_QUALITY_PRESET_DRS = 5               /* Dynamic resolution scaling (adaptive) */
} FfxVkQualityPreset;

/* Hardware capability tiers */
typedef enum FfxVkGpuTier {
    FFX_VK_GPU_TIER_GENERIC_COMPUTE = 0,    /* Standard SPIR-V compute: Polaris, Pascal, Vega, RDNA1 */
    FFX_VK_GPU_TIER_INT8_DOT4 = 1,          /* Integer dot product (DP4a): RDNA2+, Turing+, Intel Arc */
    FFX_VK_GPU_TIER_TENSOR_WMMA = 2,        /* AMD WMMA / FP8 matrix cores: RDNA3 (gfx110x), RDNA4 (gfx120x) */
    FFX_VK_GPU_TIER_TENSOR_CORES = 3        /* NVIDIA Tensor Cores: RTX 20/30/40/50 series */
} FfxVkGpuTier;

/* Known PCI Vendor IDs */
#define FFX_VK_VENDOR_AMD    0x1002u
#define FFX_VK_VENDOR_NVIDIA 0x10DEu
#define FFX_VK_VENDOR_INTEL  0x8086u
#define FFX_VK_VENDOR_ARM    0x13B5u
#define FFX_VK_VENDOR_QUALCOMM 0x5143u

/* GPU capabilities and recommendation report */
typedef struct FfxVkGpuCapabilities {
    uint32_t vendorId;
    uint32_t deviceId;
    char vendorName[32];
    char deviceName[256];
    FfxVkGpuTier tier;
    uint32_t supportedUpscalersBitmask;     /* Bitmask: (1u << FFX_VK_UPSCALER_FSR3) | ... */
    FfxVkUpscalerType recommendedUpscaler;
    bool supportsFp16;
    bool supportsInt8;
    bool supportsDot4;
    bool supportsBufferDeviceAddress;
    bool supportsFormatlessWrite;
    bool supportsWmma;
    bool supportsTensorCores;
} FfxVkGpuCapabilities;

/* Unified context creation info */
typedef struct FfxVkUnifiedSrCreateInfo {
    uint32_t structSize;
    uint32_t version;
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    FfxVkUpscalerType preferredUpscaler;    /* AUTO or explicit selection */
    FfxVkQualityPreset quality;
    VkExtent2D maxInputExtent;
    VkExtent2D maxOutputExtent;
    uint32_t flags;
} FfxVkUnifiedSrCreateInfo;

/* Unified per-frame dispatch info */
typedef struct FfxVkUnifiedSrDispatchInfo {
    uint32_t structSize;
    uint32_t version;
    VkCommandBuffer commandBuffer;
    FfxVkPortableImage colorIn;                     /* Pre-upscale HDR scene (R16G16B16A16_SFLOAT) */
    FfxVkPortableImage depth;                       /* Device depth (R32_SFLOAT) */
    FfxVkPortableImage motionVectors;               /* Normalized or pixel screen motion */
    FfxVkPortableImage exposure;                    /* Optional 1x1 exposure buffer/image */
    FfxVkPortableImage reactiveMask;                /* Optional transparency/reactivity mask */
    FfxVkPortableImage colorOut;                    /* Target display output (storage image) */
    VkExtent2D inputExtent;
    VkExtent2D outputExtent;
    float jitterOffsetX;
    float jitterOffsetY;
    float sharpness;                        /* 0.0 to 1.0 */
    float frameTimeMs;
    bool resetHistory;
} FfxVkUnifiedSrDispatchInfo;

/* Unified context handle */
typedef struct FfxVkUnifiedSrContext {
    uint32_t initialized;
    FfxVkUpscalerType activeUpscaler;
    FfxVkGpuCapabilities capabilities;
    FfxVkUnifiedSrCreateInfo createInfo;
    void *backendContext;
    FfxVkXessPipeline xessPipeline;
    FfxVkDlssPipeline dlssPipeline;
} FfxVkUnifiedSrContext;

/* Query GPU vendor, architecture tier, and recommend optimal upscaler */
bool ffxVkUnifiedSrQueryGpuCapabilities(
    VkPhysicalDevice physicalDevice,
    FfxVkGpuCapabilities *outCaps);

/* Resolve an upscaler request (handling AUTO detection and hardware fallback) */
FfxVkUpscalerType ffxVkUnifiedSrResolveUpscaler(
    FfxVkUpscalerType preferred,
    const FfxVkGpuCapabilities *caps);

/* Get human-readable upscaler name */
const char *ffxVkUnifiedSrGetUpscalerName(FfxVkUpscalerType upscaler);

/* Get human-readable GPU tier name */
const char *ffxVkUnifiedSrGetGpuTierName(FfxVkGpuTier tier);

/* Calculate input render resolution given display resolution and quality preset */
bool ffxVkUnifiedSrGetRenderResolution(
    FfxVkQualityPreset quality,
    VkExtent2D outputExtent,
    VkExtent2D *outInputExtent);

/* Calculate adaptive DRS scale given frametimes and constraints */
float ffxVkUnifiedSrCalculateDrsScale(
    float currentFrameTimeMs,
    float targetFrameTimeMs,
    float currentScale,
    float minScale,
    float maxScale);

/* Initialize unified super resolution context */
VkResult ffxVkUnifiedSrCreate(
    FfxVkUnifiedSrContext *ctx,
    const FfxVkUnifiedSrCreateInfo *info);
#define ffxVkUnifiedSrCreateContext ffxVkUnifiedSrCreate

/* Query executable readiness separately from hardware recommendation or
 * metadata-only initialization. reason, if non-NULL, receives a static string.
 * FSR3/FSR4 dispatch integration is not implemented in this wrapper yet.
 * XeSS/DLSS paths currently execute spatial prototypes, not recovered models.
 */
VkResult ffxVkUnifiedSrGetReadiness(
    const FfxVkUnifiedSrContext *ctx, const char **reason);

/* Dispatch active upscaler. VK_SUCCESS means GPU work was recorded; a context
 * initialized without a device is not a dry-run dispatch implementation. */
VkResult ffxVkUnifiedSrDispatch(
    FfxVkUnifiedSrContext *ctx,
    const FfxVkUnifiedSrDispatchInfo *dispatchInfo);

/* Destroy unified context and release resources */
void ffxVkUnifiedSrDestroy(FfxVkUnifiedSrContext *ctx);

#if defined(__cplusplus)
}
#endif

#endif /* FFX_VK_UNIFIED_SR_H */
