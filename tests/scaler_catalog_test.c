/* SPDX-License-Identifier: MIT */
#include "ffx_vk_scaler_catalog.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } } while (0)

int main(void)
{
    CHECK(!ffxVkScalerModel(-1));
    CHECK(!ffxVkScalerModel(FFX_VK_SCALER_COUNT));
    CHECK(!ffxVkScalerOption(-1));
    CHECK(!ffxVkScalerOption(FFX_VK_SCALER_OPTION_COUNT));
    CHECK(!ffxVkScalerOptionValueValid(-1, 0, 0));
    CHECK(!ffxVkScalerOptionValueValid(1, -1, 0));
    CHECK(!ffxVkScalerOptionValueValid(1, 100, 0));
    for (int m = 0; m < FFX_VK_SCALER_COUNT; ++m) {
        const FfxVkScalerModel *model = ffxVkScalerModel(m);
        CHECK((int)model->id == m && model->key[0] && model->label[0]);
        for (int n = 0; n < m; ++n)
            CHECK(strcmp(model->key, ffxVkScalerModel(n)->key));
        for (int o = 0; o < FFX_VK_SCALER_OPTION_COUNT; ++o) {
            const FfxVkScalerOption *option = ffxVkScalerOption(o);
            bool supported = ffxVkScalerHasOption(m, o);
            CHECK(ffxVkScalerOptionVisible(m, o, NULL) == supported);
            CHECK(ffxVkScalerOptionValueValid(m, o, option->defaultValue) == supported);
            CHECK(!ffxVkScalerOptionValueValid(m, o, NAN));
            CHECK(!ffxVkScalerOptionValueValid(m, o, INFINITY));
            CHECK(!ffxVkScalerOptionValueValid(m, o, option->minimum - 1));
            CHECK(!ffxVkScalerOptionValueValid(m, o, option->maximum + 1));
        }
    }
    CHECK(!ffxVkScalerHasOption(FFX_VK_SCALER_OFF, FFX_VK_SCALER_OPTION_QUALITY));
    CHECK(!ffxVkScalerHasOption(FFX_VK_SCALER_INTEL_XESS, FFX_VK_SCALER_OPTION_AUTO_EXPOSURE));
    CHECK(!ffxVkScalerHasOption(FFX_VK_SCALER_NVIDIA_DLSS, FFX_VK_SCALER_OPTION_SHARPNESS));
    CHECK(!ffxVkScalerOptionConfigurable(-1, 0, NULL, ~0u));
    CHECK(!ffxVkScalerOptionConfigurable(FFX_VK_SCALER_COUNT, 0, NULL, ~0u));
    CHECK(!ffxVkScalerOptionConfigurable(FFX_VK_SCALER_FSR4_NEURAL, 0, NULL, 0));
    CHECK(ffxVkScalerOptionConfigurable(FFX_VK_SCALER_FSR4_NEURAL, 0, NULL, 1u << FFX_VK_SCALER_FSR4_NEURAL));
    CHECK(!ffxVkScalerOptionValueValid(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_QUALITY, 1.5f));
    CHECK(!ffxVkScalerOptionValueValid(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_AUTO_EXPOSURE, 0.5f));
    CHECK(ffxVkScalerOptionValueValid(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_SHARPNESS, 0.333f));
    float settings[FFX_VK_SCALER_OPTION_COUNT] = { 2, 0.4f, 0, 1, 0 };
    CHECK(!ffxVkScalerOptionVisible(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_QUALITY, settings));
    CHECK(ffxVkScalerOptionVisible(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_SHARPNESS, settings));
    CHECK(ffxVkScalerOptionVisible(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_VARIANT, settings));
    CHECK(ffxVkScalerHasOption(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_VARIANT));
    CHECK(!ffxVkScalerHasOption(FFX_VK_SCALER_FSR3_1_4, FFX_VK_SCALER_OPTION_VARIANT));
    CHECK(ffxVkScalerOptionVisible(FFX_VK_SCALER_FSR3_1_5, FFX_VK_SCALER_OPTION_QUALITY, settings));
    settings[FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION] = 0;
    CHECK(ffxVkScalerOptionVisible(FFX_VK_SCALER_FSR4_NEURAL, FFX_VK_SCALER_OPTION_QUALITY, settings));
    CHECK(settings[FFX_VK_SCALER_OPTION_QUALITY] == 2);
    CHECK(ffxVkScalerModel(FFX_VK_SCALER_INTEL_XESS)->implementation == FFX_VK_SCALER_IMPLEMENTATION_NATIVE);
    CHECK(ffxVkScalerModel(FFX_VK_SCALER_NVIDIA_DLSS)->implementation == FFX_VK_SCALER_IMPLEMENTATION_NATIVE);
    return 0;
}
