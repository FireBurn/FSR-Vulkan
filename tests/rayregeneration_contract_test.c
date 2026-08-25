#include "ffx_vk_rayregeneration_contract.h"

#include <assert.h>
#include <string.h>

static FfxVkPortableImage image(VkFormat format) {
    return (FfxVkPortableImage){
        .structSize = sizeof(FfxVkPortableImage),
        .image = (VkImage)(uintptr_t)1,
        .format = format,
        .extent = { 960, 540 },
        .mipCount = 1,
        .arrayLayers = 1,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
        .aspect = VK_IMAGE_ASPECT_COLOR_BIT,
        .state = FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ,
    };
}

static void identity(float matrix[16]) {
    unsigned int i;
    for (i = 0; i < 16; ++i)
        matrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

int main(void) {
    FfxVkRayRegenerationInputs inputs;
    uint64_t issues = 0;
    memset(&inputs, 0, sizeof(inputs));
    inputs.structSize = sizeof(inputs);
    inputs.contractVersion = FFX_VK_RAYREGENERATION_CONTRACT_VERSION;
    inputs.signalFlags = FFX_VK_RR_SIGNAL_DIRECT_DIFFUSE |
        FFX_VK_RR_SIGNAL_INDIRECT_DIFFUSE;
    inputs.renderSize = (FfxVkPortableExtent2D){ 960, 540 };
    inputs.linearDepth = image(VK_FORMAT_R32_SFLOAT);
    inputs.motionVectors = image(VK_FORMAT_R16G16B16A16_SFLOAT);
    inputs.normalsRoughnessMaterial = image(VK_FORMAT_R8G8B8A8_UNORM);
    inputs.diffuseAlbedo = image(VK_FORMAT_R8G8B8A8_UNORM);
    inputs.specularAlbedo = image(VK_FORMAT_R8G8B8A8_UNORM);
    inputs.motionVectorScale = (FfxVkPortableFloat2){ 1.0f, 1.0f };
    identity(inputs.view);
    identity(inputs.projection);
    inputs.linearDepthMin = 0.0f;
    inputs.linearDepthMax = 10000.0f;
    inputs.directDiffuse = image(VK_FORMAT_R16G16B16A16_SFLOAT);
    inputs.indirectDiffuse = image(VK_FORMAT_R16G16B16A16_SFLOAT);
    inputs.directAlphaSemantic = FFX_VK_RR_ALPHA_NONNEGATIVE_UNDEFINED;
    inputs.indirectAlphaSemantic = FFX_VK_RR_ALPHA_FIRST_LOBE_HIT_DISTANCE;
    inputs.noHitDistance = 10000.0f;
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_RR_VALIDATION_NONE);

    inputs.indirectDiffuse.format = VK_FORMAT_R8_UNORM;
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert((issues & FFX_VK_RR_VALIDATION_IMAGE_FORMAT) != 0);

    inputs.indirectDiffuse = image(VK_FORMAT_R16G16B16A16_SFLOAT);
    inputs.signalFlags = FFX_VK_RR_SIGNAL_AMBIENT_OCCLUSION;
    inputs.ambientOcclusion = image(VK_FORMAT_R8_UNORM);
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_RR_VALIDATION_NONE);

    inputs.signalFlags = FFX_VK_RR_SIGNAL_SPECULAR_OCCLUSION;
    inputs.specularOcclusion = image(VK_FORMAT_R8_UNORM);
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_RR_VALIDATION_NONE);

    inputs.signalFlags = FFX_VK_RR_SIGNAL_AMBIENT_OCCLUSION;
    inputs.ambientOcclusion = image(VK_FORMAT_R16G16B16A16_SFLOAT);
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert((issues & FFX_VK_RR_VALIDATION_IMAGE_FORMAT) != 0);

    inputs.ambientOcclusion = image(VK_FORMAT_R8_UNORM);
    inputs.signalFlags = 1u << 31;
    assert(ffxVkRayRegenerationValidateInputs(&inputs, &issues) ==
        FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert((issues & FFX_VK_RR_VALIDATION_REQUIRED_SIGNAL) != 0);
    assert((issues & FFX_VK_RR_VALIDATION_SIGNAL_FLAGS) != 0);
    return 0;
}
