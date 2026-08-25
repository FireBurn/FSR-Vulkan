/*
 * Small, window-system-neutral policy helpers for a generated-then-real
 * Vulkan presentation pair. Applications still own acquire, submit, present,
 * fences, and swapchain lifetime.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FIFO is required for an enabled generated→real pair. Otherwise preserve the
 * normal low-latency preference: Immediate when offered, then Mailbox. */
VkPresentModeKHR ffxVkFrameGenerationSelectPresentMode(
    bool frameGenerationEnabled, bool vsyncEnabled,
    const VkPresentModeKHR *availableModes, uint32_t availableModeCount);

/* A generated and real image can both be acquired while the presentation
 * engine retains its surface minimum. Returns zero if minImageCount would
 * overflow the representable Vulkan image count. */
uint32_t ffxVkFrameGenerationRequiredImageCount(uint32_t minImageCount,
                                                bool frameGenerationEnabled);

/* Applies Vulkan's maxImageCount (zero means no limit) to the request. */
uint32_t ffxVkFrameGenerationRequestedImageCount(uint32_t minImageCount,
                                                 uint32_t maxImageCount,
                                                 bool frameGenerationEnabled);

/* Verify that two successful acquires can form a generated→real pair. */
bool ffxVkFrameGenerationValidateAcquiredPair(uint32_t generatedImageIndex,
                                              uint32_t realImageIndex,
                                              uint32_t swapchainImageCount);

/* A successful reset dispatch establishes interpolation history but has no
 * preceding image from which an intermediate image can be generated. Present
 * the real image for that paired slot and resume generation next frame. */
bool ffxVkFrameGenerationShouldPresentGenerated(bool interpolationDispatched,
                                                bool reset);

/* Render-complete binary semaphores must be owned by image and GPU, never a
 * frame-in-flight. Callers allocate imageCount * deviceCount entries. */
size_t ffxVkFrameGenerationRenderFinishedSemaphoreIndex(uint32_t imageIndex,
                                                         uint32_t gpuIndex,
                                                         uint32_t deviceCount);

#ifdef __cplusplus
}
#endif
