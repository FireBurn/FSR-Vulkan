/* SPDX-License-Identifier: MIT
 * Compiled as C: verifies ABI linkage/diagnostics without requiring model data.
 */
#include <ffx_vk_xess_recovered.h>
#include <string.h>

int xess_recovered_c_contract(void)
{
    FfxVkXeSSRecoveredContext *context = NULL;
    struct { char text[4]; unsigned char canary; } output = {{'x','x','x','x'}, 0x5a};
    if (ffxVkXeSSRecoveredCreate(NULL, &context, output.text, sizeof(output.text)) !=
            VK_ERROR_INITIALIZATION_FAILED || context != NULL ||
        output.text[3] != '\0' || output.canary != 0x5a)
        return 1;
    char tiny = 'x';
    if (ffxVkXeSSRecoveredBind(NULL, 0, NULL, &tiny, 1) != VK_ERROR_INITIALIZATION_FAILED || tiny != '\0')
        return 2;
    if (ffxVkXeSSRecoveredRecord(NULL, VK_NULL_HANDLE, NULL, 0, NULL, 0) != VK_ERROR_INITIALIZATION_FAILED ||
        ffxVkXeSSRecoveredDiscardHistory(NULL) != VK_ERROR_INITIALIZATION_FAILED ||
        ffxVkXeSSRecoveredFrameSlotCount(NULL) != 0)
        return 3;
    FfxVkXeSSRecoveredCreateInfo info = {0};
    info.structSize = sizeof(info);
    info.abiVersion = FFX_VK_XESS_RECOVERED_ABI_VERSION + 1;
    char diagnostic[128];
    if (ffxVkXeSSRecoveredCreate(&info, &context, diagnostic, sizeof(diagnostic)) !=
            VK_ERROR_INITIALIZATION_FAILED || context != NULL || strstr(diagnostic, "ABI") == NULL)
        return 4;
    ffxVkXeSSRecoveredDestroy(NULL);
    FfxVkXeSSRecoveredModel model = {0};
    if (ffxVkXeSSRecoveredModelFromBlob(NULL, 0, &model, NULL, 0) != VK_ERROR_INITIALIZATION_FAILED)
        return 5;
    FfxVkXeSSReplayExtent extent = {0};
    if (!ffxVkXeSSRecoveredGetExtent((VkExtent2D){1920,1080}, &extent) ||
        extent.inputWidth != 1130 || extent.inputHeight != 636)
        return 6;
    if (ffxVkXeSSRecoveredGetExtent((VkExtent2D){1920,1079}, &extent) || extent.inputWidth || extent.outputWidth)
        return 7;
    if (ffxVkXeSSRecoveredBindConverted(NULL, 0, NULL, NULL, diagnostic, sizeof(diagnostic)) != VK_ERROR_INITIALIZATION_FAILED)
        return 8;
    return 0;
}
#ifdef FFX_XESS_C_CONTRACT_STANDALONE
int main(void) { return xess_recovered_c_contract(); }
#endif
