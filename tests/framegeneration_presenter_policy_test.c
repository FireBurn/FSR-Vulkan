#include "ffx_vk_framegeneration_presenter.h"

#include <stdio.h>

static int failures;

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); \
        ++failures; \
    } \
} while (0)

int main(void)
{
    const VkPresentModeKHR immediate[] = { VK_PRESENT_MODE_FIFO_KHR,
                                           VK_PRESENT_MODE_IMMEDIATE_KHR };
    const VkPresentModeKHR mailbox[] = { VK_PRESENT_MODE_FIFO_KHR,
                                         VK_PRESENT_MODE_MAILBOX_KHR };

    CHECK(ffxVkFrameGenerationSelectPresentMode(true, false, immediate, 2) ==
          VK_PRESENT_MODE_FIFO_KHR);
    CHECK(ffxVkFrameGenerationSelectPresentMode(false, true, immediate, 2) ==
          VK_PRESENT_MODE_FIFO_KHR);
    CHECK(ffxVkFrameGenerationSelectPresentMode(false, false, immediate, 2) ==
          VK_PRESENT_MODE_IMMEDIATE_KHR);
    CHECK(ffxVkFrameGenerationSelectPresentMode(false, false, mailbox, 2) ==
          VK_PRESENT_MODE_MAILBOX_KHR);

    CHECK(ffxVkFrameGenerationRequiredImageCount(3, true) == 5);
    CHECK(ffxVkFrameGenerationRequiredImageCount(3, false) == 3);
    CHECK(ffxVkFrameGenerationRequestedImageCount(3, 0, true) == 5);
    CHECK(ffxVkFrameGenerationRequestedImageCount(3, 4, true) == 4);
    CHECK(ffxVkFrameGenerationRequestedImageCount(1, 0, false) == 2);

    CHECK(ffxVkFrameGenerationValidateAcquiredPair(0, 1, 4));
    CHECK(!ffxVkFrameGenerationValidateAcquiredPair(0, 0, 4));
    CHECK(!ffxVkFrameGenerationValidateAcquiredPair(0, 4, 4));
    CHECK(!ffxVkFrameGenerationValidateAcquiredPair(0, 1, 1));

    CHECK(ffxVkFrameGenerationRenderFinishedSemaphoreIndex(3, 0, 2) == 6u);
    CHECK(ffxVkFrameGenerationRenderFinishedSemaphoreIndex(3, 1, 2) == 7u);
    CHECK(ffxVkFrameGenerationRenderFinishedSemaphoreIndex(0, 1, 0) == SIZE_MAX);
    CHECK(ffxVkFrameGenerationRenderFinishedSemaphoreIndex(0, 2, 2) == SIZE_MAX);
    return failures ? 1 : 0;
}
