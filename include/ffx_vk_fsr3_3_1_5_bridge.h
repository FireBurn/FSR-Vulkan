/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#ifndef FFX_VK_FSR3_3_1_5_BRIDGE_H
#define FFX_VK_FSR3_3_1_5_BRIDGE_H

#include <vulkan/vulkan.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Opaque implementation state used by the SDK 2.3 FfxInterface Vulkan
 * callbacks.  It intentionally contains no application-owned resources: the
 * embedding application supplies those through RegisterResource later in the
 * bridge.  Pipelines use the checked embedded Q2-compatible 3.1.5 modules.
 */
typedef struct FfxVkFsr3_3_1_5Bridge FfxVkFsr3_3_1_5Bridge;
typedef struct FfxVkFsr3_3_1_5UpscalerContext FfxVkFsr3_3_1_5UpscalerContext;

/* Opaque at this boundary: Q2RTX's v07 provider has a separate set of
 * legacy FfxApi* declarations, so leaking the public SDK types here prevents
 * both providers from coexisting in one renderer translation unit. */
typedef struct FfxVkFsr3_3_1_5Resource {
    void* resource;
} FfxVkFsr3_3_1_5Resource;

typedef struct FfxVkFsr3_3_1_5SharedResourceDescription {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
} FfxVkFsr3_3_1_5SharedResourceDescription;

enum {
    FFX_VK_FSR3_3_1_5_RESOURCE_STATE_COMPUTE_READ = (1u << 2),
    FFX_VK_FSR3_3_1_5_RESOURCE_STATE_UNORDERED_ACCESS = (1u << 1),
};

typedef enum FfxVkFsr3_3_1_5Result {
    FFX_VK_FSR3_3_1_5_OK = 0,
    FFX_VK_FSR3_3_1_5_ERROR_INVALID_ARGUMENT = -1,
    FFX_VK_FSR3_3_1_5_ERROR_OUT_OF_MEMORY = -2,
    FFX_VK_FSR3_3_1_5_ERROR_BACKEND = -3,
} FfxVkFsr3_3_1_5Result;

typedef struct FfxVkFsr3_3_1_5UpscalerCreateInfo {
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    uint32_t maxRenderWidth;
    uint32_t maxRenderHeight;
    uint32_t maxUpscaleWidth;
    uint32_t maxUpscaleHeight;
    VkBool32 hdrColorInput;
    VkBool32 autoExposure;
} FfxVkFsr3_3_1_5UpscalerCreateInfo;

typedef struct FfxVkFsr3_3_1_5UpscalerDispatchInfo {
    VkCommandBuffer commandBuffer;
    FfxVkFsr3_3_1_5Resource color;
    FfxVkFsr3_3_1_5Resource depth;
    FfxVkFsr3_3_1_5Resource motionVectors;
    FfxVkFsr3_3_1_5Resource exposure;
    FfxVkFsr3_3_1_5Resource reactive;
    FfxVkFsr3_3_1_5Resource transparencyAndComposition;
    FfxVkFsr3_3_1_5Resource dilatedDepth;
    FfxVkFsr3_3_1_5Resource dilatedMotionVectors;
    FfxVkFsr3_3_1_5Resource reconstructedPrevNearestDepth;
    FfxVkFsr3_3_1_5Resource output;
    float jitterOffsetX;
    float jitterOffsetY;
    float motionVectorScaleX;
    float motionVectorScaleY;
    uint32_t renderWidth;
    uint32_t renderHeight;
    uint32_t upscaleWidth;
    uint32_t upscaleHeight;
    float frameTimeMilliseconds;
    float preExposure;
    VkBool32 enableSharpening;
    float sharpness;
    VkBool32 reset;
    float cameraNear;
    float cameraFar;
    float cameraVerticalFovRadians;
    float viewSpaceToMeters;
    uint32_t flags;
} FfxVkFsr3_3_1_5UpscalerDispatchInfo;

typedef struct FfxVkFsr3_3_1_5SharedResourceDescriptions {
    FfxVkFsr3_3_1_5SharedResourceDescription dilatedDepth;
    FfxVkFsr3_3_1_5SharedResourceDescription dilatedMotionVectors;
    FfxVkFsr3_3_1_5SharedResourceDescription reconstructedPrevNearestDepth;
} FfxVkFsr3_3_1_5SharedResourceDescriptions;

/* An application-owned 2D image supplied to the SDK 3.1.5 scheduler.  The
 * caller retains VkImage and memory ownership.  `layout` must describe the
 * image at import time and will be restored after each scheduler dispatch;
 * `state` is the matching FFX abstract state supplied in the resulting API
 * resource.  The bridge creates only temporary VkImageViews. */
typedef struct FfxVkFsr3_3_1_5ImportedImageDescription {
    VkImage image;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;
    uint32_t arrayLayers;
    VkImageLayout layout;
    uint32_t state;
    VkImageUsageFlags usage;
} FfxVkFsr3_3_1_5ImportedImageDescription;

FfxVkFsr3_3_1_5Bridge* ffxVkFsr3_3_1_5CreateBridge(
    VkDevice device, const VkAllocationCallbacks* allocationCallbacks);

/* Required for SDK-owned image/buffer allocation callbacks.  The narrower
 * CreateBridge form remains useful for pipeline-only verification. */
FfxVkFsr3_3_1_5Bridge* ffxVkFsr3_3_1_5CreateBridgeWithPhysicalDevice(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    const VkAllocationCallbacks* allocationCallbacks);

void ffxVkFsr3_3_1_5DestroyBridge(FfxVkFsr3_3_1_5Bridge* bridge);

/* Resolve an API resource token returned by the bridge to its native image.
 * The bridge retains ownership; callers may use this only while the resource
 * and bridge remain alive.  It is primarily useful for portable diagnostics,
 * readback, and an embedding application's explicit synchronization layer. */
VkImage ffxVkFsr3_3_1_5BridgeGetNativeImage(
    FfxVkFsr3_3_1_5Bridge* bridge, const void* resourceToken);

/* Import a caller-owned image and return the exact FFX resource token to put
 * into an FSR3.1.5 dispatch description.  Failure is represented by a zeroed
 * FfxVkFsr3_3_1_5Resource.  Release only destroys bridge-owned views; it never destroys
 * the caller's image or memory. */
FfxVkFsr3_3_1_5Resource ffxVkFsr3_3_1_5BridgeImportImage(
    FfxVkFsr3_3_1_5Bridge* bridge,
    const FfxVkFsr3_3_1_5ImportedImageDescription* description);

void ffxVkFsr3_3_1_5BridgeReleaseImportedImage(
    FfxVkFsr3_3_1_5Bridge* bridge, FfxVkFsr3_3_1_5Resource resource);

/* A small C lifecycle layer around the public SDK scheduler.  Context creation
 * queues the SDK's immutable-resource uploads; the first RecordDispatch emits
 * them before frame work in the caller command buffer, preserving Vulkan queue
 * ordering without an internal submit or queue wait. */
FfxVkFsr3_3_1_5Result ffxVkFsr3_3_1_5UpscalerContextCreate(
    const FfxVkFsr3_3_1_5UpscalerCreateInfo* createInfo,
    FfxVkFsr3_3_1_5UpscalerContext** outContext);

FfxVkFsr3_3_1_5Bridge* ffxVkFsr3_3_1_5UpscalerContextGetBridge(
    FfxVkFsr3_3_1_5UpscalerContext* context);

FfxVkFsr3_3_1_5Result ffxVkFsr3_3_1_5UpscalerContextGetSharedResourceDescriptions(
    FfxVkFsr3_3_1_5UpscalerContext* context,
    FfxVkFsr3_3_1_5SharedResourceDescriptions* outDescriptions);

FfxVkFsr3_3_1_5Result ffxVkFsr3_3_1_5UpscalerContextRecordDispatch(
    FfxVkFsr3_3_1_5UpscalerContext* context,
    const FfxVkFsr3_3_1_5UpscalerDispatchInfo* dispatchInfo);

void ffxVkFsr3_3_1_5UpscalerContextDestroy(
    FfxVkFsr3_3_1_5UpscalerContext* context);

#if defined(__cplusplus)
}
#endif

#endif
