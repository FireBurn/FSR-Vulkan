/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_rayregeneration_contract.h"

#include <math.h>

static int image_is_readable(const FfxVkPortableImage* image,
                             FfxVkPortableExtent2D render_size,
                             VkFormat format_a, VkFormat format_b,
                             uint64_t* issues) {
    if (image->structSize != sizeof(*image)) {
        *issues |= FFX_VK_RR_VALIDATION_STRUCT_SIZE;
        return 0;
    }
    if (image->image == VK_NULL_HANDLE) {
        *issues |= FFX_VK_RR_VALIDATION_IMAGE_HANDLE;
        return 0;
    }
    if (image->extent.width < render_size.width ||
        image->extent.height < render_size.height) {
        *issues |= FFX_VK_RR_VALIDATION_IMAGE_EXTENT;
    }
    if ((image->usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0) {
        *issues |= FFX_VK_RR_VALIDATION_IMAGE_USAGE;
    }
    if (image->state != FFX_VK_PORTABLE_RESOURCE_STATE_GENERIC_READ &&
        image->state != FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ) {
        *issues |= FFX_VK_RR_VALIDATION_IMAGE_STATE;
    }
    if (image->format != format_a && image->format != format_b) {
        *issues |= FFX_VK_RR_VALIDATION_IMAGE_FORMAT;
    }
    return 1;
}

static int finite_float3(FfxVkPortableFloat3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static int finite_float2(FfxVkPortableFloat2 value) {
    return isfinite(value.x) && isfinite(value.y);
}

static int finite_matrix(const float matrix[16]) {
    uint32_t i;
    for (i = 0; i < 16; ++i) {
        if (!isfinite(matrix[i]))
            return 0;
    }
    return 1;
}

FfxVkPortableResult ffxVkRayRegenerationValidateInputs(
    const FfxVkRayRegenerationInputs* inputs, uint64_t* issues) {
    uint64_t result = FFX_VK_RR_VALIDATION_NONE;
    const uint32_t known_signals = FFX_VK_RR_SIGNAL_DIRECT_DIFFUSE |
        FFX_VK_RR_SIGNAL_DIRECT_SPECULAR |
        FFX_VK_RR_SIGNAL_INDIRECT_DIFFUSE |
        FFX_VK_RR_SIGNAL_INDIRECT_SPECULAR |
        FFX_VK_RR_SIGNAL_DOMINANT_LIGHT_VISIBILITY |
        FFX_VK_RR_SIGNAL_AMBIENT_OCCLUSION |
        FFX_VK_RR_SIGNAL_SPECULAR_OCCLUSION;
    const uint32_t primary_signals = FFX_VK_RR_SIGNAL_DIRECT_DIFFUSE |
        FFX_VK_RR_SIGNAL_DIRECT_SPECULAR |
        FFX_VK_RR_SIGNAL_INDIRECT_DIFFUSE |
        FFX_VK_RR_SIGNAL_INDIRECT_SPECULAR |
        FFX_VK_RR_SIGNAL_DOMINANT_LIGHT_VISIBILITY;

    if (!issues)
        return FFX_VK_PORTABLE_ERROR_INVALID_POINTER;
    *issues = FFX_VK_RR_VALIDATION_NONE;
    if (!inputs)
        return FFX_VK_PORTABLE_ERROR_INVALID_POINTER;
    if (inputs->structSize != sizeof(*inputs) ||
        inputs->contractVersion != FFX_VK_RAYREGENERATION_CONTRACT_VERSION)
        result |= FFX_VK_RR_VALIDATION_STRUCT_SIZE;
    if (!inputs->renderSize.width || !inputs->renderSize.height)
        result |= FFX_VK_RR_VALIDATION_ZERO_EXTENT;

    /* AO and specular occlusion are optional additions. A real RR provider
     * requires at least one primary radiance or dominant-light signal. */
    if (!(inputs->signalFlags & primary_signals))
        result |= FFX_VK_RR_VALIDATION_REQUIRED_SIGNAL;
    if (inputs->signalFlags & ~known_signals)
        result |= FFX_VK_RR_VALIDATION_SIGNAL_FLAGS;
    image_is_readable(&inputs->linearDepth, inputs->renderSize,
        VK_FORMAT_R32_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    image_is_readable(&inputs->motionVectors, inputs->renderSize,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    image_is_readable(&inputs->normalsRoughnessMaterial, inputs->renderSize,
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_A2B10G10R10_UNORM_PACK32, &result);
    image_is_readable(&inputs->diffuseAlbedo, inputs->renderSize,
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, &result);
    image_is_readable(&inputs->specularAlbedo, inputs->renderSize,
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_UNDEFINED, &result);
    if (!finite_float3(inputs->motionVectorScale) ||
        !finite_float2(inputs->jitterOffset) ||
        !finite_float3(inputs->cameraPositionDelta) ||
        !finite_matrix(inputs->view) || !finite_matrix(inputs->projection) ||
        !isfinite(inputs->linearDepthMin) || !isfinite(inputs->linearDepthMax))
        result |= FFX_VK_RR_VALIDATION_NONFINITE_METADATA;
    if (inputs->motionVectorScale.x == 0.0f ||
        inputs->motionVectorScale.y == 0.0f ||
        inputs->motionVectorScale.z == 0.0f ||
        inputs->linearDepthMin < 0.0f ||
        inputs->linearDepthMax <= inputs->linearDepthMin)
        result |= FFX_VK_RR_VALIDATION_CAMERA_METADATA;

    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_DIRECT_DIFFUSE) != 0)
        image_is_readable(&inputs->directDiffuse, inputs->renderSize,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_DIRECT_SPECULAR) != 0)
        image_is_readable(&inputs->directSpecular, inputs->renderSize,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_INDIRECT_DIFFUSE) != 0)
        image_is_readable(&inputs->indirectDiffuse, inputs->renderSize,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_INDIRECT_SPECULAR) != 0)
        image_is_readable(&inputs->indirectSpecular, inputs->renderSize,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_AMBIENT_OCCLUSION) != 0)
        image_is_readable(&inputs->ambientOcclusion, inputs->renderSize,
            VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED, &result);
    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_SPECULAR_OCCLUSION) != 0)
        image_is_readable(&inputs->specularOcclusion, inputs->renderSize,
            VK_FORMAT_R8_UNORM, VK_FORMAT_UNDEFINED, &result);
    if (inputs->directAlphaSemantic != FFX_VK_RR_ALPHA_NONNEGATIVE_UNDEFINED ||
        inputs->indirectAlphaSemantic != FFX_VK_RR_ALPHA_FIRST_LOBE_HIT_DISTANCE ||
        !isfinite(inputs->noHitDistance) || inputs->noHitDistance <= 0.0f)
        result |= FFX_VK_RR_VALIDATION_ALPHA_SEMANTIC;

    if ((inputs->signalFlags & FFX_VK_RR_SIGNAL_DOMINANT_LIGHT_VISIBILITY) != 0) {
        image_is_readable(&inputs->dominantLightVisibility, inputs->renderSize,
            VK_FORMAT_R16_SFLOAT, VK_FORMAT_UNDEFINED, &result);
        if (!finite_float3(inputs->dominantLightDirection) ||
            !finite_float3(inputs->dominantLightEmission) ||
            !isfinite(inputs->dominantLightAngularRadius) ||
            inputs->dominantLightAngularRadius <= 0.0f ||
            (inputs->dominantLightDirection.x * inputs->dominantLightDirection.x +
             inputs->dominantLightDirection.y * inputs->dominantLightDirection.y +
             inputs->dominantLightDirection.z * inputs->dominantLightDirection.z) < 1e-6f)
            result |= FFX_VK_RR_VALIDATION_DOMINANT_LIGHT;
    }

    *issues = result;
    return result == FFX_VK_RR_VALIDATION_NONE ? FFX_VK_PORTABLE_OK
                                                 : FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;
}
