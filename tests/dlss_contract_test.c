/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_dlss_contract.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static FfxVkPortableImage make_image(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage) {
    FfxVkPortableImage img;
    memset(&img, 0, sizeof(img));
    img.structSize = sizeof(FfxVkPortableImage);
    img.image = (VkImage)(uintptr_t)1;
    img.format = format;
    img.extent.width = width;
    img.extent.height = height;
    img.mipCount = 1;
    img.arrayLayers = 1;
    img.usage = usage;
    img.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    img.state = FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ;
    return img;
}

int main(void) {
    uint64_t issues = 0;
    FfxVkPortableExtent2D display = { 2560, 1440 };
    FfxVkPortableExtent2D render = { 1280, 720 };
    FfxVkPortableExtent2D optimal = { 0, 0 };

    /* Test resolution calculation for presets */
    assert(ffxVkDlssGetOptimalRenderResolution(display, FFX_VK_DLSS_PRESET_PERFORMANCE, &optimal) == FFX_VK_PORTABLE_OK);
    assert(optimal.width == 1280 && optimal.height == 720);

    assert(ffxVkDlssGetOptimalRenderResolution(display, FFX_VK_DLSS_PRESET_QUALITY, &optimal) == FFX_VK_PORTABLE_OK);
    assert(optimal.width == 1707 && optimal.height == 960);

    assert(ffxVkDlssGetOptimalRenderResolution(display, FFX_VK_DLSS_PRESET_DLAA, &optimal) == FFX_VK_PORTABLE_OK);
    assert(optimal.width == 2560 && optimal.height == 1440);

    /* Test model and arch names */
    assert(strcmp(ffxVkDlssGetModelName(FFX_VK_DLSS_MODEL_4_5_TRANSFORMER_M), "DLSS 4.5 Transformer (Model M)") == 0);
    assert(strcmp(ffxVkDlssGetModelName(FFX_VK_DLSS_MODEL_5_NEURAL_RENDERING), "DLSS 5 Neural Rendering (DLSSNR)") == 0);
    assert(strcmp(ffxVkDlssGetArchName(FFX_VK_DLSS_ARCH_RDNA3), "AMD RDNA3 (gfx110x WMMA FP16)") == 0);
    assert(strcmp(ffxVkDlssGetArchName(FFX_VK_DLSS_ARCH_RDNA4), "AMD RDNA4 (gfx120x Native FP8 WMMA)") == 0);

    /* 1. Test valid DLSS 4 / d4r on RDNA3 create info */
    FfxVkDlssCreateInfo create_info;
    memset(&create_info, 0, sizeof(create_info));
    create_info.structSize = sizeof(FfxVkDlssCreateInfo);
    create_info.contractVersion = FFX_VK_DLSS_CONTRACT_VERSION;
    create_info.model = FFX_VK_DLSS_MODEL_4_SWIN_K;
    create_info.preset = FFX_VK_DLSS_PRESET_PERFORMANCE;
    create_info.flags = FFX_VK_DLSS_FLAG_NATIVE_SWIN_ENCODERS | FFX_VK_DLSS_FLAG_VRAM_INTEROP;
    create_info.gpuArch = FFX_VK_DLSS_ARCH_RDNA3;
    create_info.maxRenderSize = render;
    create_info.displaySize = display;
    create_info.motionVectorDilation = 2;

    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_DLSS_VALIDATION_NONE);

    /* Test create info validation failures */
    create_info.contractVersion = 999;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_CONTRACT_VERSION);
    create_info.contractVersion = FFX_VK_DLSS_CONTRACT_VERSION;

    create_info.displaySize.width = 0;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_ZERO_EXTENT);
    create_info.displaySize = display;

    /* 2. Test valid DLSS 4 dispatch info */
    FfxVkDlssDispatchInfo dispatch_info;
    memset(&dispatch_info, 0, sizeof(dispatch_info));
    dispatch_info.structSize = sizeof(FfxVkDlssDispatchInfo);
    dispatch_info.contractVersion = FFX_VK_DLSS_CONTRACT_VERSION;
    dispatch_info.flags = FFX_VK_DLSS_FLAG_VRAM_INTEROP | FFX_VK_DLSS_FLAG_AUTO_EXPOSURE;
    dispatch_info.renderSize = render;

    dispatch_info.color = make_image(render.width, render.height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.depth = make_image(render.width, render.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.motionVectors = make_image(render.width, render.height, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.output = make_image(display.width, display.height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT);

    dispatch_info.motionVectorScale.x = 1.0f / (float)render.width;
    dispatch_info.motionVectorScale.y = 1.0f / (float)render.height;
    dispatch_info.verticalFov = 1.047f; /* 60 deg */
    dispatch_info.nearZ = 0.1f;
    dispatch_info.farZ = 1000.0f;
    dispatch_info.preExposure = 1.0f;

    /* d4r VRAM interop handles */
    FfxVkDlssExternalMemoryHandle color_handle = {
        .structSize = sizeof(FfxVkDlssExternalMemoryHandle),
        .memory = (VkDeviceMemory)(uintptr_t)1,
        .size = 1280 * 720 * 8,
        .memoryTypeIndex = 0,
        .fd = 42,
        .win32Handle = NULL
    };
    FfxVkDlssExternalMemoryHandle out_handle = {
        .structSize = sizeof(FfxVkDlssExternalMemoryHandle),
        .memory = (VkDeviceMemory)(uintptr_t)2,
        .size = 2560 * 1440 * 8,
        .memoryTypeIndex = 0,
        .fd = 43,
        .win32Handle = NULL
    };
    dispatch_info.colorExportHandle = &color_handle;
    dispatch_info.outputExportHandle = &out_handle;

    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_DLSS_VALIDATION_NONE);

    /* 3. Test non-finite value rejection */
    dispatch_info.jitterOffset.x = NAN;
    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_NONFINITE_VALUE);
    dispatch_info.jitterOffset.x = 0.0f;

    dispatch_info.verticalFov = -1.0f;
    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_CAMERA_METADATA);
    dispatch_info.verticalFov = 1.047f;

    /* 4. Test DLSS 5 Neural Rendering validation */
    create_info.model = FFX_VK_DLSS_MODEL_5_NEURAL_RENDERING;
    create_info.flags |= FFX_VK_DLSS_FLAG_NATIVE_FP8;
    create_info.gpuArch = FFX_VK_DLSS_ARCH_RDNA4;

    /* Missing G-buffer & radiance signals initially */
    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_NEURAL_RENDERING_MISSING_SIGNAL);
    assert(issues & FFX_VK_DLSS_VALIDATION_NEURAL_RENDERING_WEIGHTS);

    /* Populate DLSS 5 Neural Rendering inputs */
    dispatch_info.normalsRoughnessMaterial = make_image(render.width, render.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.diffuseAlbedo = make_image(render.width, render.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.specularAlbedo = make_image(render.width, render.height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.directDiffuse = make_image(render.width, render.height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);
    dispatch_info.indirectDiffuse = make_image(render.width, render.height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT);

    dispatch_info.weightsTensorBuffer.structSize = sizeof(FfxVkPortableBuffer);
    dispatch_info.weightsTensorBuffer.buffer = (VkBuffer)(uintptr_t)1;
    dispatch_info.weightsTensorBuffer.size = 147 * 1024 * 1024; /* ~147 MB */
    dispatch_info.weightsTensorBuffer.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    dispatch_info.weightsTensorBuffer.state = FFX_VK_PORTABLE_RESOURCE_STATE_COMPUTE_READ;

    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_DLSS_VALIDATION_NONE);

    /* If indirect diffuse has wrong format (e.g. not RGBA16F containing hit distance) */
    dispatch_info.indirectDiffuse.format = VK_FORMAT_R8G8B8A8_UNORM;
    assert(ffxVkDlssValidateDispatchInfo(&create_info, &dispatch_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_IMAGE_FORMAT);
    dispatch_info.indirectDiffuse.format = VK_FORMAT_R16G16B16A16_SFLOAT;

    /* 5. Test embedded SPIR-V binary query and pipeline API safety */
    size_t spirvWordCount = 0;
    const uint32_t *spirvWords = ffxVkDlssGetReconstructSpirv(&spirvWordCount);
    assert(spirvWords != NULL);
    assert(spirvWordCount > 0);
    assert(spirvWords[0] == 0x07230203u); /* SPIR-V magic number */

    const uint32_t *preWords = ffxVkDlssGetInputPreprocessSpirv(&spirvWordCount);
    assert(preWords != NULL && spirvWordCount > 0 && preWords[0] == 0x07230203u);

    const uint32_t *encWords = ffxVkDlssGetSwinEncoderSpirv(&spirvWordCount);
    assert(encWords != NULL && spirvWordCount > 0 && encWords[0] == 0x07230203u);

    const uint32_t *decWords = ffxVkDlssGetSwinDecoderSpirv(&spirvWordCount);
    assert(decWords != NULL && spirvWordCount > 0 && decWords[0] == 0x07230203u);

    const uint32_t *dfnWords = ffxVkDlssGetDfnReconstructSpirv(&spirvWordCount);
    assert(dfnWords != NULL && spirvWordCount > 0 && dfnWords[0] == 0x07230203u);

    /* Null device or pipeline pointer fail safely */
    assert(ffxVkDlssCreatePipeline(VK_NULL_HANDLE, NULL) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    FfxVkDlssPipeline dummyPipeline;
    assert(ffxVkDlssCreatePipeline(VK_NULL_HANDLE, &dummyPipeline) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    ffxVkDlssDestroyPipeline(VK_NULL_HANDLE, &dummyPipeline);
    assert(ffxVkDlssExecuteDispatch(VK_NULL_HANDLE, &dummyPipeline, &dispatch_info) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    FfxVkDlssIntermediateResources dummyIntermediates;
    memset(&dummyIntermediates, 0, sizeof(dummyIntermediates));
    assert(ffxVkDlssExecuteMultiPassDispatch(VK_NULL_HANDLE, &dummyPipeline, &dispatch_info, &dummyIntermediates) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);

    /* 6. Test offline pre-trained tensor weights container ingestion (DLSSMOD1 and DLSSNR1) */
    size_t dlssModelSize = 65536;
    uint8_t* dlssModelBuf = (uint8_t*)calloc(1, dlssModelSize);
    assert(dlssModelBuf != NULL);
    memcpy(dlssModelBuf, "DLSSMOD1", 8);
    uint32_t dlssVer = 1, dlssFamily = FFX_VK_DLSS_MODEL_4_SWIN_K;
    memcpy(dlssModelBuf + 8, &dlssVer, sizeof(dlssVer));
    memcpy(dlssModelBuf + 12, &dlssFamily, sizeof(dlssFamily));

    /* Reset create_info to DLSS 4 Swin model */
    create_info.model = FFX_VK_DLSS_MODEL_4_SWIN_K;
    create_info.modelContainerData = dlssModelBuf;
    create_info.modelContainerSizeBytes = dlssModelSize;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_DLSS_VALIDATION_NONE);

    memset(&dummyPipeline, 0, sizeof(dummyPipeline));
    assert(ffxVkDlssPipelineSetModel(&dummyPipeline, dlssModelBuf, dlssModelSize) == FFX_VK_PORTABLE_OK);
    assert(dummyPipeline.hasPretrainedWeights == true);
    assert(dummyPipeline.weightsSizeBytes == dlssModelSize);

    /* Truncated DLSSMOD1 container is rejected */
    create_info.modelContainerSizeBytes = 1024;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_MODEL_WEIGHTS_TRUNCATED);
    assert(ffxVkDlssPipelineSetModel(&dummyPipeline, dlssModelBuf, 1024) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);

    /* Invalid magic is rejected */
    dlssModelBuf[0] = 'X';
    create_info.modelContainerSizeBytes = dlssModelSize;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_MODEL_CONTAINER_INVALID);
    assert(ffxVkDlssPipelineSetModel(&dummyPipeline, dlssModelBuf, dlssModelSize) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    free(dlssModelBuf);

    /* DLSSNR1 Neural Rendering weights container */
    size_t dlssnrModelSize = 1048576;
    uint8_t* dlssnrModelBuf = (uint8_t*)calloc(1, dlssnrModelSize);
    assert(dlssnrModelBuf != NULL);
    memcpy(dlssnrModelBuf, "DLSSNR1\0", 8);
    uint32_t dlssnrVer = 1;
    memcpy(dlssnrModelBuf + 8, &dlssnrVer, sizeof(dlssnrVer));

    create_info.model = FFX_VK_DLSS_MODEL_5_NEURAL_RENDERING;
    create_info.modelContainerData = dlssnrModelBuf;
    create_info.modelContainerSizeBytes = dlssnrModelSize;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_OK);
    assert(issues == FFX_VK_DLSS_VALIDATION_NONE);

    memset(&dummyPipeline, 0, sizeof(dummyPipeline));
    assert(ffxVkDlssPipelineSetModel(&dummyPipeline, dlssnrModelBuf, dlssnrModelSize) == FFX_VK_PORTABLE_OK);
    assert(dummyPipeline.hasPretrainedWeights == true);
    assert(dummyPipeline.weightsSizeBytes == dlssnrModelSize);

    /* Truncated DLSSNR1 container is rejected */
    create_info.modelContainerSizeBytes = 512;
    assert(ffxVkDlssValidateCreateInfo(&create_info, &issues) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);
    assert(issues & FFX_VK_DLSS_VALIDATION_MODEL_WEIGHTS_TRUNCATED);
    assert(ffxVkDlssPipelineSetModel(&dummyPipeline, dlssnrModelBuf, 512) == FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT);

    free(dlssnrModelBuf);
    create_info.modelContainerData = NULL;
    create_info.modelContainerSizeBytes = 0;

    return 0;
}

