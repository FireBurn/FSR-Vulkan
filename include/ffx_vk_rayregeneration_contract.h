/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FFX_VK_RAYREGENERATION_CONTRACT_H
#define FFX_VK_RAYREGENERATION_CONTRACT_H

#include "ffx_vk_portable.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* A provider-neutral Vulkan input contract for modern decoupled ray denoisers.
 * It does not contain AMD neural kernels or an RR dispatch implementation.
 * Hosts can use it to validate their signals before attaching a legally
 * available provider on a supported platform. */
#define FFX_VK_RAYREGENERATION_CONTRACT_VERSION 1u

typedef enum FfxVkRayRegenerationSignalFlagBits {
    FFX_VK_RR_SIGNAL_DIRECT_DIFFUSE = 1u << 0,
    FFX_VK_RR_SIGNAL_DIRECT_SPECULAR = 1u << 1,
    FFX_VK_RR_SIGNAL_INDIRECT_DIFFUSE = 1u << 2,
    FFX_VK_RR_SIGNAL_INDIRECT_SPECULAR = 1u << 3,
    FFX_VK_RR_SIGNAL_DOMINANT_LIGHT_VISIBILITY = 1u << 4
} FfxVkRayRegenerationSignalFlagBits;

typedef enum FfxVkRayRegenerationAlphaSemantic {
    /* Required for direct radiance: non-negative but otherwise undefined. */
    FFX_VK_RR_ALPHA_NONNEGATIVE_UNDEFINED = 0,
    /* Required for indirect radiance: first selected-lobe ray distance;
     * negative indicates that the lobe was not traced for that pixel. */
    FFX_VK_RR_ALPHA_FIRST_LOBE_HIT_DISTANCE = 1
} FfxVkRayRegenerationAlphaSemantic;

typedef enum FfxVkRayRegenerationValidationIssueBits {
    FFX_VK_RR_VALIDATION_NONE = 0,
    FFX_VK_RR_VALIDATION_STRUCT_SIZE = 1ull << 0,
    FFX_VK_RR_VALIDATION_ZERO_EXTENT = 1ull << 1,
    FFX_VK_RR_VALIDATION_REQUIRED_SIGNAL = 1ull << 2,
    FFX_VK_RR_VALIDATION_IMAGE_HANDLE = 1ull << 3,
    FFX_VK_RR_VALIDATION_IMAGE_EXTENT = 1ull << 4,
    FFX_VK_RR_VALIDATION_IMAGE_USAGE = 1ull << 5,
    FFX_VK_RR_VALIDATION_IMAGE_STATE = 1ull << 6,
    FFX_VK_RR_VALIDATION_IMAGE_FORMAT = 1ull << 7,
    FFX_VK_RR_VALIDATION_ALPHA_SEMANTIC = 1ull << 8,
    FFX_VK_RR_VALIDATION_DOMINANT_LIGHT = 1ull << 9,
    FFX_VK_RR_VALIDATION_NONFINITE_METADATA = 1ull << 10,
    FFX_VK_RR_VALIDATION_CAMERA_METADATA = 1ull << 11
} FfxVkRayRegenerationValidationIssueBits;

typedef struct FfxVkRayRegenerationInputs {
    uint32_t structSize;
    uint32_t contractVersion;
    uint32_t signalFlags;
    FfxVkPortableExtent2D renderSize;

    /* R32_SFLOAT signed linear depth and RGBA16F motion where XYZ encode
     * PreviousUV-CurrentUV and PreviousDepth-CurrentDepth respectively. */
    FfxVkPortableImage linearDepth;
    FfxVkPortableImage motionVectors;
    /* RGBA8_UNORM or A2B10G10R10_UNORM_PACK32: oct normal, linear roughness,
     * normalized 0..3 material class. Albedos are sqrt encoded RGBA8. */
    FfxVkPortableImage normalsRoughnessMaterial;
    FfxVkPortableImage diffuseAlbedo;
    FfxVkPortableImage specularAlbedo;

    /* Camera metadata follows Vulkan column-major / column-vector convention.
     * Motion vectors are scaled before use; jitter is current-frame jitter in
     * render-pixel units. Depth bounds describe the positive ray-tracing
     * range, even though linearDepth may hold signed virtual-reflection depth. */
    FfxVkPortableFloat2 motionVectorScale;
    FfxVkPortableFloat2 jitterOffset;
    FfxVkPortableFloat3 cameraPositionDelta;
    float view[16];
    float projection[16];
    float linearDepthMin;
    float linearDepthMax;

    /* Active radiance signals are RGBA16F. Direct alpha is non-negative and
     * undefined; indirect alpha is the first selected-lobe hit distance. */
    FfxVkPortableImage directDiffuse;
    FfxVkPortableImage directSpecular;
    FfxVkPortableImage indirectDiffuse;
    FfxVkPortableImage indirectSpecular;
    uint32_t directAlphaSemantic;
    uint32_t indirectAlphaSemantic;
    float noHitDistance;

    /* Optional R16_SFLOAT dominant-light hit distance. FP16_MAX means fully
     * exposed; direction is from source to target and angularRadius is radians. */
    FfxVkPortableImage dominantLightVisibility;
    FfxVkPortableFloat3 dominantLightDirection;
    FfxVkPortableFloat3 dominantLightEmission;
    float dominantLightAngularRadius;
} FfxVkRayRegenerationInputs;

FfxVkPortableResult ffxVkRayRegenerationValidateInputs(
    const FfxVkRayRegenerationInputs* inputs, uint64_t* issues);

#if defined(__cplusplus)
}
#endif

#endif
