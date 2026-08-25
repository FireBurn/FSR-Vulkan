/*
 * Compile/link contract for a downstream Vulkan application.  This intentionally
 * does not create a device: applications own instance/device/queue creation and
 * pass the features they actually enabled into the FSR3 and FSR4 create calls.
 */
#include <ffx_vk_fsr4_v07.h>
#include <ffx_vk_fsr4_v07_assets.h>
#include <ffx_vk_framegeneration_presenter.h>

int main(void)
{
    FfxFsr4VkCreateInfo fsr4_backend = {0};
    FfxFsr4V07AssetSet fsr4_assets;
    FfxInterface interface = {0};

    /* A real application fills its Vulkan handles and enabled feature bits. */
    fsr4_backend.device = VK_NULL_HANDLE;

    /* Asset selection is independent from filesystem and Vulkan ownership. */
    if (!ffxFsr4V07BuildAssetSet(FFX_FSR4_MODEL_PRESET_QUALITY,
                                 1920u, 1080u, &fsr4_assets))
        return 1;

    /* Keep WSI ownership in the host while sharing generated→real invariants. */
    if (ffxVkFrameGenerationRequiredImageCount(3u, true) != 5u ||
        !ffxVkFrameGenerationValidateAcquiredPair(0u, 1u, 5u) ||
        ffxVkFrameGenerationRenderFinishedSemaphoreIndex(1u, 0u, 1u) != 1u)
        return 2;

    /* Context creation copies the caller's interface; no Q2RTX global exists. */
    ffxFsr4V07SetBackendInterface(&interface);
    ffxFsr4V07SetBackendInterface(NULL);
    return 0;
}
