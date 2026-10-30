/* SPDX-License-Identifier: MIT
 * Offline configuration/readiness example. This does not execute inference.
 */
#include <ffx_vk_unified_sr.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    VkExtent2D displayExtent = { 2560, 1440 };
    VkExtent2D renderExtent = { 0, 0 };
    if (!ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_QUALITY,
            displayExtent, &renderExtent))
        return 1;
    printf("Quality input %ux%u -> display %ux%u\n", renderExtent.width,
           renderExtent.height, displayExtent.width, displayExtent.height);

    FfxVkUnifiedSrCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.structSize = sizeof(createInfo);
    createInfo.version = FFX_VK_UNIFIED_SR_VERSION;
    createInfo.preferredUpscaler = FFX_VK_UPSCALER_AUTO;
    createInfo.quality = FFX_VK_QUALITY_PRESET_QUALITY;
    createInfo.maxInputExtent = renderExtent;
    createInfo.maxOutputExtent = displayExtent;
    /* A real consumer supplies its Vulkan device and physical device. */
    FfxVkUnifiedSrContext context;
    VkResult result = ffxVkUnifiedSrCreate(&context, &createInfo);
    if (result != VK_SUCCESS)
        return 1;

    const char *reason = NULL;
    result = ffxVkUnifiedSrGetReadiness(&context, &reason);
    printf("Recommendation: %s\nReadiness: %s\n",
        ffxVkUnifiedSrGetUpscalerName(context.activeUpscaler), reason);
    ffxVkUnifiedSrDestroy(&context);
    if (result == VK_SUCCESS) {
        fprintf(stderr, "Offline context must not advertise executable readiness\n");
        return 1;
    }
    printf("No GPU work recorded. Check readiness before dispatching real resources.\n");
    return 0;
}
