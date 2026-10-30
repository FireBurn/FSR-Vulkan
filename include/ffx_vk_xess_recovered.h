/* SPDX-License-Identifier: MIT
 * Experimental C interface to the recovered XeSS quality-profile graph.
 * Model/shader bytes are supplied by the caller, never a vendor runtime DLL.
 */
#ifndef FFX_VK_XESS_RECOVERED_H
#define FFX_VK_XESS_RECOVERED_H
#include <vulkan/vulkan.h>
#include <stddef.h>
#include <stdint.h>
#include "ffx_vk_xess_replay_constants.h"

#ifdef __cplusplus
extern "C" {
#endif
#define FFX_VK_XESS_RECOVERED_ABI_VERSION 1u
typedef struct FfxVkXeSSRecoveredContext FfxVkXeSSRecoveredContext;

typedef struct FfxVkXeSSRecoveredBlob {
    const void *data;
    size_t sizeBytes;
} FfxVkXeSSRecoveredBlob;

typedef struct FfxVkXeSSRecoveredConvolution {
    FfxVkXeSSRecoveredBlob shader;
    FfxVkXeSSRecoveredBlob weights, scales, biases;
} FfxVkXeSSRecoveredConvolution;

typedef struct FfxVkXeSSRecoveredModel {
    uint32_t structSize;
    FfxVkXeSSRecoveredBlob prepareShader, resolveShader;
    FfxVkXeSSRecoveredConvolution convolutions[12];
    /* Resolve CBVs include padding: 512, 256 and 256 bytes. */
    FfxVkXeSSRecoveredBlob resolveWeights, resolveScales, resolveBiases;
} FfxVkXeSSRecoveredModel;

typedef struct FfxVkXeSSRecoveredCreateInfo {
    uint32_t structSize;
    uint32_t abiVersion;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    FfxVkXeSSReplayExtent extent;
    uint32_t frameSlots; /* 1..8; each slot must retire before rebinding. */
    const FfxVkXeSSRecoveredModel *model;
} FfxVkXeSSRecoveredCreateInfo;

typedef struct FfxVkXeSSRecoveredImage {
    VkImage image;
    VkImageView view;
    VkExtent2D extent;
    VkFormat format;
} FfxVkXeSSRecoveredImage;

typedef struct FfxVkXeSSRecoveredImages {
    uint32_t structSize;
    FfxVkXeSSRecoveredImage color, motion, depth, output;
} FfxVkXeSSRecoveredImages;

typedef struct FfxVkXeSSRecoveredConversion {
    uint32_t structSize;
    float colorScale;   /* Engine stored colour -> model linear HDR. */
    float motionScaleX, motionScaleY; /* Engine motion -> input pixels. */
    float outputScale;  /* Model linear HDR -> engine stored colour. */
} FfxVkXeSSRecoveredConversion;

/* Caller owns device, images, command buffers and submissions. Inputs must be
 * GENERAL sampled 2D views; output must be GENERAL storage RGBA16F. Current
 * profile: unit exposure, linear HDR colour, pixel motion (previous-current),
 * conventional device depth. See the C++ context documentation for enabled
 * device features/limits. The provider performs no queue waits/submissions.
 * All calls on a context are externally synchronized. Submit recordings once
 * in temporal order on one queue. Retire every use before destroy/discard.
 * A provided diagnostic buffer with nonzero capacity is NUL terminated.
 * A failure sets *outContext to NULL; successful creation copies model bytes,
 * so the caller can immediately release all model/shader storage.
 * Blob validation checks bounds/header shape, not shader semantics; payloads
 * and external image descriptions are trusted caller inputs.
 */
VkResult ffxVkXeSSRecoveredCreate(const FfxVkXeSSRecoveredCreateInfo *info,
    FfxVkXeSSRecoveredContext **outContext, char *diagnostic, size_t capacity);
/* Decode an XESSVK1 container without GPU work or copying payloads. CRC32, canonical
 * table bounds and entry count are checked. The returned spans borrow blob
 * storage, which must live until Create returns. Failure clears outModel.
 * This is container validation; Create also validates model/header sizes.
 */
VkResult ffxVkXeSSRecoveredModelFromBlob(const void *blob, size_t sizeBytes,
    FfxVkXeSSRecoveredModel *outModel, char *diagnostic, size_t capacity);
void ffxVkXeSSRecoveredDestroy(FfxVkXeSSRecoveredContext *context);
VkResult ffxVkXeSSRecoveredBind(FfxVkXeSSRecoveredContext *context,
    uint32_t frameSlot, const FfxVkXeSSRecoveredImages *images,
    char *diagnostic, size_t capacity);
/* Optional GPU conversion: color is RGBA16F; motion can be RG16F or RGBA16F
 * (only XY is used). Alpha metadata is replaced by 1 before reconstruction.
 * Extents describe the valid top-left rectangles; views may cover larger
 * allocations. All three inputs are cropped into exact model-sized images.
 * Per-slot intermediate images/pipelines are owned by the provider. Recording
 * adds conversion before the graph and output scaling afterward. Layout and
 * producer/consumer synchronization requirements remain the caller's.
 */
VkResult ffxVkXeSSRecoveredBindConverted(FfxVkXeSSRecoveredContext *context,
    uint32_t frameSlot, const FfxVkXeSSRecoveredImages *images,
    const FfxVkXeSSRecoveredConversion *conversion, char *diagnostic, size_t capacity);
VkResult ffxVkXeSSRecoveredRecord(FfxVkXeSSRecoveredContext *context,
    VkCommandBuffer commands, const FfxVkXeSSReplayFrame *frame,
    uint32_t frameSlot, char *diagnostic, size_t capacity);
VkResult ffxVkXeSSRecoveredDiscardHistory(FfxVkXeSSRecoveredContext *context);
uint32_t ffxVkXeSSRecoveredFrameSlotCount(const FfxVkXeSSRecoveredContext *context);
/* Exact enabled profile dimensions, not a readiness/device-support query. */
VkBool32 ffxVkXeSSRecoveredGetExtent(VkExtent2D output, FfxVkXeSSReplayExtent *extent);
#ifdef __cplusplus
}
#endif
#endif
