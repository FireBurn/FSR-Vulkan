/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FFX_VK_DLSS_CONTRACT_H
#define FFX_VK_DLSS_CONTRACT_H

#include "ffx_vk_portable.h"

#if defined(__cplusplus)
extern "C" {
#endif

/* Provider-neutral Vulkan contract for NVIDIA DLSS (including d4r on AMD Radeon)
 * and DLSS 5 Neural Rendering. It validates host application images, metadata,
 * GPU architecture requirements, and VRAM interop handles before dispatch.
 * It is easily usable by any Vulkan application without requiring Wine or DX12. */
#define FFX_VK_DLSS_CONTRACT_VERSION 1u

/* DLSS model families.
 * Models E, K, M, L are DLSS 3 / 4 / 4.5 super resolution models supported on
 * AMD RDNA3/RDNA4 via d4r (countervolts/d4r) and on NVIDIA RTX.
 * Model 5 is DLSS 5 Neural Rendering / Reconstruction (NVNGX DLSSNR). */
typedef enum FfxVkDlssModel {
    FFX_VK_DLSS_MODEL_3_CNN_E = 0,               /* DLSS 3 CNN model (preset E) */
    FFX_VK_DLSS_MODEL_4_SWIN_K = 1,              /* DLSS 4 Swin transformer (preset K) */
    FFX_VK_DLSS_MODEL_4_5_TRANSFORMER_M = 2,     /* DLSS 4.5 transformer (preset M) */
    FFX_VK_DLSS_MODEL_4_5_TRANSFORMER_L = 3,     /* DLSS 4.5 Ultra Performance transformer (preset L) */
    FFX_VK_DLSS_MODEL_5_NEURAL_RENDERING = 4      /* DLSS 5 Neural Rendering / Reconstruction */
} FfxVkDlssModel;

typedef enum FfxVkDlssPreset {
    FFX_VK_DLSS_PRESET_ULTRA_PERFORMANCE = 0,    /* 3.0x scale (e.g. 853x480 -> 2560x1440) */
    FFX_VK_DLSS_PRESET_PERFORMANCE = 1,          /* 2.0x scale (e.g. 1280x720 -> 2560x1440) */
    FFX_VK_DLSS_PRESET_BALANCED = 2,             /* 1.7x scale (e.g. 1488x837 -> 2560x1440) */
    FFX_VK_DLSS_PRESET_QUALITY = 3,              /* 1.5x scale (e.g. 1705x960 -> 2560x1440) */
    FFX_VK_DLSS_PRESET_DLAA = 4                  /* 1.0x native anti-aliasing */
} FfxVkDlssPreset;

typedef enum FfxVkDlssGpuArch {
    FFX_VK_DLSS_ARCH_UNKNOWN = 0,
    FFX_VK_DLSS_ARCH_NVIDIA_TENSOR = 1,          /* NVIDIA sm_80, sm_89, sm_120 with Tensor Cores */
    FFX_VK_DLSS_ARCH_RDNA3 = 2,                  /* AMD RDNA3 gfx1100-gfx1103 with WMMA FP16 */
    FFX_VK_DLSS_ARCH_RDNA4 = 3,                  /* AMD RDNA4 gfx1200-gfx1201 with native FP8 WMMA */
    FFX_VK_DLSS_ARCH_GENERIC_VULKAN = 4          /* Vulkan VK_KHR_cooperative_matrix / subgroup matrix */
} FfxVkDlssGpuArch;

typedef enum FfxVkDlssFlagBits {
    /* d4r accuracy variant: restores conservative arithmetic matching NVIDIA reference */
    FFX_VK_DLSS_FLAG_PREFER_ACCURACY = 1u << 0,
    /* d4r native Swin encoders: use hand-written RDNA3/RDNA4 Swin layer kernels */
    FFX_VK_DLSS_FLAG_NATIVE_SWIN_ENCODERS = 1u << 1,
    /* Zero-copy VRAM interop: inputs/output remain in GPU memory across Vulkan and CUDA/HIP */
    FFX_VK_DLSS_FLAG_VRAM_INTEROP = 1u << 2,
    /* Direct output: DLSS output kernel directly writes the game-side output buffer (preset K) */
    FFX_VK_DLSS_FLAG_DIRECT_OUTPUT = 1u << 3,
    /* Elide sync: skip CPU-side context synchronization checks for lower CPU latency */
    FFX_VK_DLSS_FLAG_ELIDE_SYNC = 1u << 4,
    /* Native FP8 arithmetic on supported hardware (RDNA4 gfx120x, NVIDIA Blackwell sm_120) */
    FFX_VK_DLSS_FLAG_NATIVE_FP8 = 1u << 5,
    /* Automatic internal exposure computation if exposure image is not provided */
    FFX_VK_DLSS_FLAG_AUTO_EXPOSURE = 1u << 6,
    /* Input color is linear HDR */
    FFX_VK_DLSS_FLAG_HDR_INPUT = 1u << 7,
    /* Inverted depth convention: 1.0 at near plane, 0.0 at far plane */
    FFX_VK_DLSS_FLAG_INVERTED_DEPTH = 1u << 8
} FfxVkDlssFlagBits;

typedef enum FfxVkDlssValidationIssueBits {
    FFX_VK_DLSS_VALIDATION_NONE = 0,
    FFX_VK_DLSS_VALIDATION_STRUCT_SIZE = 1ull << 0,
    FFX_VK_DLSS_VALIDATION_CONTRACT_VERSION = 1ull << 1,
    FFX_VK_DLSS_VALIDATION_ZERO_EXTENT = 1ull << 2,
    FFX_VK_DLSS_VALIDATION_EXTENT_EXCEEDS_MAX = 1ull << 3,
    FFX_VK_DLSS_VALIDATION_IMAGE_HANDLE = 1ull << 4,
    FFX_VK_DLSS_VALIDATION_IMAGE_FORMAT = 1ull << 5,
    FFX_VK_DLSS_VALIDATION_IMAGE_USAGE = 1ull << 6,
    FFX_VK_DLSS_VALIDATION_IMAGE_STATE = 1ull << 7,
    FFX_VK_DLSS_VALIDATION_NONFINITE_VALUE = 1ull << 8,
    FFX_VK_DLSS_VALIDATION_CAMERA_METADATA = 1ull << 9,
    FFX_VK_DLSS_VALIDATION_UNSUPPORTED_ARCH = 1ull << 10,
    FFX_VK_DLSS_VALIDATION_UNSUPPORTED_MODEL = 1ull << 11,
    FFX_VK_DLSS_VALIDATION_NEURAL_RENDERING_MISSING_SIGNAL = 1ull << 12,
    FFX_VK_DLSS_VALIDATION_NEURAL_RENDERING_WEIGHTS = 1ull << 13,
    FFX_VK_DLSS_VALIDATION_VRAM_INTEROP_HANDLE = 1ull << 14,
    FFX_VK_DLSS_VALIDATION_MODEL_CONTAINER_INVALID = 1ull << 15,
    FFX_VK_DLSS_VALIDATION_MODEL_WEIGHTS_TRUNCATED = 1ull << 16
} FfxVkDlssValidationIssueBits;

/* External memory handle for zero-copy VRAM interop between Vulkan and d4r / CUDA / HIP */
typedef struct FfxVkDlssExternalMemoryHandle {
    uint32_t structSize;
    VkDeviceMemory memory;
    uint64_t size;
    uint32_t memoryTypeIndex;
    int fd;               /* POSIX opaque file descriptor, or -1 if unused */
    void* win32Handle;    /* Win32 HANDLE, or NULL if unused */
} FfxVkDlssExternalMemoryHandle;

typedef struct FfxVkDlssCreateInfo {
    uint32_t structSize;
    uint32_t contractVersion;
    FfxVkDlssModel model;
    FfxVkDlssPreset preset;
    uint32_t flags;
    FfxVkDlssGpuArch gpuArch;
    FfxVkPortableExtent2D maxRenderSize;
    FfxVkPortableExtent2D displaySize;
    /* Optional motion vector dilation window (0 = default 2-pixel dilation) */
    uint32_t motionVectorDilation;
    /* Optional pointer to packed DLSSMOD1 / DLSSNR1 model container in host memory */
    const void* modelContainerData;
    size_t modelContainerSizeBytes;
} FfxVkDlssCreateInfo;

typedef struct FfxVkDlssDispatchInfo {
    uint32_t structSize;
    uint32_t contractVersion;
    uint32_t flags;
    FfxVkPortableExtent2D renderSize;

    /* Core temporal upscaling inputs */
    FfxVkPortableImage color;
    FfxVkPortableImage depth;
    FfxVkPortableImage motionVectors;
    FfxVkPortableImage exposure;          /* Optional if FFX_VK_DLSS_FLAG_AUTO_EXPOSURE is set */
    FfxVkPortableImage reactiveMask;      /* Optional R8_UNORM reactive mask */
    FfxVkPortableImage output;            /* Reconstructed output (must match displaySize) */

    /* Camera and temporal metadata */
    FfxVkPortableFloat2 jitterOffset;
    FfxVkPortableFloat2 motionVectorScale;
    FfxVkPortableFloat3 cameraPositionDelta;
    float verticalFov;
    float nearZ;
    float farZ;
    float preExposure;                    /* 1.0f if unused */
    VkBool32 frameReset;                  /* VK_TRUE on camera cut, scene transition, or resize */

    /* Optional d4r VRAM interop handles (for zero-copy GPU memory sharing) */
    const FfxVkDlssExternalMemoryHandle* colorExportHandle;
    const FfxVkDlssExternalMemoryHandle* outputExportHandle;

    /* Extended DLSS 5 Neural Rendering inputs (required when model == FFX_VK_DLSS_MODEL_5_NEURAL_RENDERING) */
    FfxVkPortableImage normalsRoughnessMaterial; /* Oct-normal, roughness, material class */
    FfxVkPortableImage diffuseAlbedo;            /* Sqrt diffuse albedo RGBA8 */
    FfxVkPortableImage specularAlbedo;           /* Sqrt specular albedo RGBA8 */
    FfxVkPortableImage directDiffuse;            /* Linear direct diffuse radiance */
    FfxVkPortableImage directSpecular;           /* Linear direct specular radiance */
    FfxVkPortableImage indirectDiffuse;          /* Linear indirect diffuse; alpha = first hit distance */
    FfxVkPortableImage indirectSpecular;         /* Linear indirect specular; alpha = first hit distance */
    FfxVkPortableImage dominantLightBlockerDistance; /* Optional R16F blocker distance */
    FfxVkPortableBuffer weightsTensorBuffer;     /* Tensor weight buffer containing WEIGHTS_HT blobs */
} FfxVkDlssDispatchInfo;

/* Validate create info parameters fail-closed before allocating resources. */
FfxVkPortableResult ffxVkDlssValidateCreateInfo(
    const FfxVkDlssCreateInfo* createInfo, uint64_t* issues);

/* Validate dispatch parameters and input/output images before recording GPU work. */
FfxVkPortableResult ffxVkDlssValidateDispatchInfo(
    const FfxVkDlssCreateInfo* createInfo,
    const FfxVkDlssDispatchInfo* dispatchInfo, uint64_t* issues);

/* Calculate optimal input resolution for a display extent and DLSS preset. */
FfxVkPortableResult ffxVkDlssGetOptimalRenderResolution(
    FfxVkPortableExtent2D displaySize,
    FfxVkDlssPreset preset,
    FfxVkPortableExtent2D* outRenderSize);

/* Return human-readable model and architecture names. */
const char* ffxVkDlssGetModelName(FfxVkDlssModel model);
const char* ffxVkDlssGetArchName(FfxVkDlssGpuArch arch);

#define FFX_VK_DLSS_DESCRIPTOR_SET_COUNT 4
#define FFX_VK_DLSS_STAGE_COUNT 4

/* Stage enum for multi-pass neural reconstruction */
typedef enum FfxVkDlssStage {
    FFX_VK_DLSS_STAGE_PREPROCESS = 0,
    FFX_VK_DLSS_STAGE_ENCODER = 1,
    FFX_VK_DLSS_STAGE_DECODER = 2,
    FFX_VK_DLSS_STAGE_DFN_RECONSTRUCT = 3,
    FFX_VK_DLSS_STAGE_SINGLE_PASS = 4
} FfxVkDlssStage;

/* Intermediate texture resources for 4-stage neural pipeline execution */
typedef struct FfxVkDlssIntermediateResources {
    FfxVkPortableImage preprocessedFeatures; /* RGBA16F (renderSize) */
    FfxVkPortableImage dilatedMotion;        /* RG16F (renderSize) */
    FfxVkPortableImage colorVariance;        /* RGBA16F (renderSize) */
    FfxVkPortableImage latentFeatures;       /* RGBA16F (renderSize) */
    FfxVkPortableImage decodedFeatures;      /* RGBA16F (displaySize) */
    FfxVkPortableImage historyIn;            /* RGBA16F (displaySize) */
    FfxVkPortableImage historyOut;           /* RGBA16F (displaySize) */
} FfxVkDlssIntermediateResources;

/* DLSS / d4r Native Compute Pipeline and Descriptor State */
typedef struct FfxVkDlssPipeline {
    VkDevice device;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSets[FFX_VK_DLSS_DESCRIPTOR_SET_COUNT];
    VkSampler linearSampler;
    VkShaderModule shaderModule;
    uint32_t currentSetIndex;
    bool hasPretrainedWeights;
    size_t weightsSizeBytes;

    /* Multi-pass d4r neural compute pipeline stages */
    bool multiPassAvailable;
    VkPipeline stagePipelines[FFX_VK_DLSS_STAGE_COUNT];
    VkPipelineLayout stagePipelineLayouts[FFX_VK_DLSS_STAGE_COUNT];
    VkDescriptorSetLayout stageDescriptorSetLayouts[FFX_VK_DLSS_STAGE_COUNT];
    VkShaderModule stageShaderModules[FFX_VK_DLSS_STAGE_COUNT];
    VkDescriptorSet stageDescriptorSets[FFX_VK_DLSS_STAGE_COUNT][FFX_VK_DLSS_DESCRIPTOR_SET_COUNT];
} FfxVkDlssPipeline;

/* Access the embedded DLSS / d4r SPIR-V binaries */
const uint32_t* ffxVkDlssGetReconstructSpirv(size_t* outWordCount);
const uint32_t* ffxVkDlssGetInputPreprocessSpirv(size_t* outWordCount);
const uint32_t* ffxVkDlssGetSwinEncoderSpirv(size_t* outWordCount);
const uint32_t* ffxVkDlssGetSwinDecoderSpirv(size_t* outWordCount);
const uint32_t* ffxVkDlssGetDfnReconstructSpirv(size_t* outWordCount);

/* Create compute pipeline for DLSS / d4r Swin Transformer reconstruction pass */
FfxVkPortableResult ffxVkDlssCreatePipeline(
    VkDevice device,
    FfxVkDlssPipeline* outPipeline);

/* Destroy compute pipeline resources */
void ffxVkDlssDestroyPipeline(
    VkDevice device,
    FfxVkDlssPipeline* pipeline);

/* Bind offline pre-trained tensor weights container (DLSSMOD1 / DLSSNR1) */
FfxVkPortableResult ffxVkDlssPipelineSetModel(
    FfxVkDlssPipeline* pipeline,
    const void* modelContainerData,
    size_t modelContainerSizeBytes);

/* Execute DLSS / d4r Swin Transformer reconstruction compute dispatch (fast single-pass) */
FfxVkPortableResult ffxVkDlssExecuteDispatch(
    VkCommandBuffer cmdBuf,
    FfxVkDlssPipeline* pipeline,
    const FfxVkDlssDispatchInfo* dispatchInfo);

/* Execute full 4-stage neural compute pipeline (preprocess, encoder, decoder, DFN reconstruct) */
FfxVkPortableResult ffxVkDlssExecuteMultiPassDispatch(
    VkCommandBuffer cmdBuf,
    FfxVkDlssPipeline* pipeline,
    const FfxVkDlssDispatchInfo* dispatchInfo,
    const FfxVkDlssIntermediateResources* intermediates);

#if defined(__cplusplus)
}
#endif

#endif /* FFX_VK_DLSS_CONTRACT_H */
