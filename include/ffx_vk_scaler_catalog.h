/* SPDX-License-Identifier: MIT
 * Engine-independent scaler settings. No Vulkan device or UI is required.
 * Stable model IDs preserve the existing master-scaler API values.
 */
#ifndef FFX_VK_SCALER_CATALOG_H
#define FFX_VK_SCALER_CATALOG_H

#include <stdbool.h>
#include <stddef.h>
#include <math.h>

typedef enum FfxVkMasterScalerId {
    FFX_VK_SCALER_OFF = 0,
    FFX_VK_SCALER_FSR3_1_4 = 1,
    FFX_VK_SCALER_FSR4_NEURAL = 2,
    FFX_VK_SCALER_FSR3_1_5 = 3,
    FFX_VK_SCALER_INTEL_XESS = 4,
    FFX_VK_SCALER_NVIDIA_DLSS = 5,
    FFX_VK_SCALER_UNIFIED_AUTO = 6,
    FFX_VK_SCALER_COUNT = 7
} FfxVkMasterScalerId;

typedef enum FfxVkScalerOptionId {
    FFX_VK_SCALER_OPTION_QUALITY,
    FFX_VK_SCALER_OPTION_SHARPNESS,
    FFX_VK_SCALER_OPTION_AUTO_EXPOSURE,
    FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION,
    FFX_VK_SCALER_OPTION_COUNT
} FfxVkScalerOptionId;

typedef enum FfxVkScalerOptionType {
    FFX_VK_SCALER_OPTION_ENUM,
    FFX_VK_SCALER_OPTION_RANGE,
    FFX_VK_SCALER_OPTION_BOOL
} FfxVkScalerOptionType;

typedef enum FfxVkScalerImplementation {
    FFX_VK_SCALER_IMPLEMENTATION_NONE,
    FFX_VK_SCALER_IMPLEMENTATION_NATIVE,
    FFX_VK_SCALER_IMPLEMENTATION_SPATIAL_PROTOTYPE,
    FFX_VK_SCALER_IMPLEMENTATION_AUTO
} FfxVkScalerImplementation;

typedef struct FfxVkScalerChoice {
    const char *label;
    int value;
} FfxVkScalerChoice;

typedef struct FfxVkScalerOption {
    FfxVkScalerOptionId id;
    const char *key; /* Stable persistence key, not an engine cvar. */
    const char *label;
    const char *description;
    FfxVkScalerOptionType type;
    float defaultValue, minimum, maximum, step;
    const FfxVkScalerChoice *choices;
    size_t choiceCount;
} FfxVkScalerOption;

typedef struct FfxVkScalerModel {
    FfxVkMasterScalerId id;
    const char *key;
    const char *label;
    const char *description;
    FfxVkScalerImplementation implementation;
    unsigned optionMask;
} FfxVkScalerModel;

/* Describes implemented paths, not a claim of device support. Consumers must
 * query pipeline creation/capabilities and report the active fallback/reason.
 * Prototype entries deliberately do not claim recovered vendor inference.
 */
static inline const FfxVkScalerModel *ffxVkScalerModel(int id)
{
    static const FfxVkScalerModel models[] = {
        { FFX_VK_SCALER_OFF, "off", "Off / Native", "Use the engine's native rendering and anti-aliasing.", FFX_VK_SCALER_IMPLEMENTATION_NONE, 0 },
        { FFX_VK_SCALER_FSR3_1_4, "fsr314", "FSR 3.1.4", "FSR 3.1.4 temporal reconstruction.", FFX_VK_SCALER_IMPLEMENTATION_NATIVE, 3 },
        { FFX_VK_SCALER_FSR4_NEURAL, "fsr4", "FSR 4", "FSR4 v07 native Vulkan model pipeline. Device and asset availability are checked at runtime.", FFX_VK_SCALER_IMPLEMENTATION_NATIVE, 15 },
        { FFX_VK_SCALER_FSR3_1_5, "fsr315", "FSR 3.1.5", "FSR 3.1.5 temporal reconstruction.", FFX_VK_SCALER_IMPLEMENTATION_NATIVE, 3 },
        { FFX_VK_SCALER_INTEL_XESS, "xess", "XeSS 2.0.2 (Quality)", "Recovered XeSS 2.0.2 Quality INT8/DP4a U-Net (540p/720p/1080p) with spatial fallback at other presets.", FFX_VK_SCALER_IMPLEMENTATION_NATIVE, 3 },
        { FFX_VK_SCALER_NVIDIA_DLSS, "dlss", "DLSS spatial prototype", "Experimental spatial filter with fixed sharpening; recovered DLSS4/5 neural model execution is not implemented.", FFX_VK_SCALER_IMPLEMENTATION_SPATIAL_PROTOTYPE, 1 },
        { FFX_VK_SCALER_UNIFIED_AUTO, "auto", "Automatic", "Select a backend at runtime. See active-provider diagnostics for the result and fallback reason.", FFX_VK_SCALER_IMPLEMENTATION_AUTO, 3 }
    };
    return id >= 0 && id < FFX_VK_SCALER_COUNT ? &models[id] : NULL;
}

static inline const FfxVkScalerOption *ffxVkScalerOption(int id)
{
    static const FfxVkScalerChoice quality[] = {
        { "Native AA", 0 }, { "Quality", 1 }, { "Balanced", 2 },
        { "Performance", 3 }, { "Ultra Performance", 4 }
    };
    static const FfxVkScalerChoice boolean[] = { { "Off", 0 }, { "On", 1 } };
    static const FfxVkScalerOption options[] = {
        { FFX_VK_SCALER_OPTION_QUALITY, "quality", "quality", "Input resolution: Native AA 100%, Quality 67%, Balanced 59%, Performance 50%, Ultra Performance 33%.", FFX_VK_SCALER_OPTION_ENUM, 3, 0, 4, 1, quality, 5 },
        { FFX_VK_SCALER_OPTION_SHARPNESS, "sharpness", "sharpening", "Reconstruction sharpening amount; zero disables sharpening.", FFX_VK_SCALER_OPTION_RANGE, 0, 0, 1, 0.05f, NULL, 0 },
        { FFX_VK_SCALER_OPTION_AUTO_EXPOSURE, "auto_exposure", "auto exposure", "Use the model's automatic exposure. Changing this resets temporal history.", FFX_VK_SCALER_OPTION_BOOL, 0, 0, 1, 1, boolean, 2 },
        { FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION, "dynamic_resolution", "dynamic-resolution model", "Use the dynamic-resolution model instead of a fixed quality preset. Configure the engine's resolution controller separately.", FFX_VK_SCALER_OPTION_BOOL, 0, 0, 1, 1, boolean, 2 }
    };
    return id >= 0 && id < FFX_VK_SCALER_OPTION_COUNT ? &options[id] : NULL;
}

static inline bool ffxVkScalerHasOption(int model, int option)
{
    const FfxVkScalerModel *m = ffxVkScalerModel(model);
    return m && option >= 0 && option < FFX_VK_SCALER_OPTION_COUNT &&
        (m->optionMask & (1u << option)) != 0;
}

/* values is indexed by FfxVkScalerOptionId; NULL uses descriptor defaults. */
static inline bool ffxVkScalerOptionVisible(int model, int option, const float *values)
{
    if (!ffxVkScalerHasOption(model, option))
        return false;
    if (option == FFX_VK_SCALER_OPTION_QUALITY &&
        ffxVkScalerHasOption(model, FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION)) {
        float dynamic = values ? values[FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION] :
            ffxVkScalerOption(FFX_VK_SCALER_OPTION_DYNAMIC_RESOLUTION)->defaultValue;
        return dynamic == 0;
    }
    return true;
}

static inline bool ffxVkScalerOptionValueValid(int model, int option, float value)
{
    const FfxVkScalerOption *o = ffxVkScalerOption(option);
    if (!ffxVkScalerHasOption(model, option) || !isfinite(value) ||
        value < o->minimum || value > o->maximum)
        return false;
    if (o->type == FFX_VK_SCALER_OPTION_RANGE)
        return true; /* step is a UI increment, not quantization of inference. */
    for (size_t i = 0; i < o->choiceCount; ++i)
        if (value == (float)o->choices[i].value)
            return true;
    return false;
}

/* readyModels comes from successfully created runtime contexts, not PCI
 * vendor IDs or theoretical hardware support. Unavailable choices can remain
 * listed with a reason, but must not expose ineffective tuning controls. */
static inline bool ffxVkScalerOptionConfigurable(int model, int option,
    const float *values, unsigned readyModels)
{
    return ffxVkScalerModel(model) && (readyModels & (1u << model)) != 0 &&
        ffxVkScalerOptionVisible(model, option, values);
}

#endif
