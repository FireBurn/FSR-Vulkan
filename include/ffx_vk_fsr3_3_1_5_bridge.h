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

#if defined(__cplusplus)
}
#endif

#endif
