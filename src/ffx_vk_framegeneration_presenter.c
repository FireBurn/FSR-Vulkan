#include "ffx_vk_framegeneration_presenter.h"

#include <limits.h>

VkPresentModeKHR ffxVkFrameGenerationSelectPresentMode(
    bool frameGenerationEnabled, bool vsyncEnabled,
    const VkPresentModeKHR *availableModes, uint32_t availableModeCount)
{
    if (frameGenerationEnabled || vsyncEnabled)
        return VK_PRESENT_MODE_FIFO_KHR;

    for (uint32_t i = 0; i < availableModeCount; ++i)
        if (availableModes && availableModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
    return VK_PRESENT_MODE_MAILBOX_KHR;
}

uint32_t ffxVkFrameGenerationRequiredImageCount(uint32_t minImageCount,
                                                bool frameGenerationEnabled)
{
    if (!frameGenerationEnabled)
        return minImageCount > 2u ? minImageCount : 2u;
    if (minImageCount > UINT32_MAX - 2u)
        return 0;
    return minImageCount + 2u;
}

uint32_t ffxVkFrameGenerationRequestedImageCount(uint32_t minImageCount,
                                                 uint32_t maxImageCount,
                                                 bool frameGenerationEnabled)
{
    uint32_t requested = ffxVkFrameGenerationRequiredImageCount(
        minImageCount, frameGenerationEnabled);
    if (!requested)
        return 0;
    if (maxImageCount && requested > maxImageCount)
        requested = maxImageCount;
    return requested < minImageCount ? minImageCount : requested;
}

bool ffxVkFrameGenerationValidateAcquiredPair(uint32_t generatedImageIndex,
                                              uint32_t realImageIndex,
                                              uint32_t swapchainImageCount)
{
    return swapchainImageCount > 1u &&
           generatedImageIndex < swapchainImageCount &&
           realImageIndex < swapchainImageCount &&
           generatedImageIndex != realImageIndex;
}

bool ffxVkFrameGenerationShouldPresentGenerated(bool interpolationDispatched,
                                                bool reset)
{
    return interpolationDispatched && !reset;
}

size_t ffxVkFrameGenerationRenderFinishedSemaphoreIndex(uint32_t imageIndex,
                                                         uint32_t gpuIndex,
                                                         uint32_t deviceCount)
{
    if (!deviceCount || gpuIndex >= deviceCount)
        return SIZE_MAX;
    if ((size_t)imageIndex > (SIZE_MAX - (size_t)gpuIndex) / (size_t)deviceCount)
        return SIZE_MAX;
    return (size_t)imageIndex * (size_t)deviceCount + (size_t)gpuIndex;
}
