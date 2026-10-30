#include "ffx_vk_fsr4_v07_schedule.h"

#include <limits.h>
#include <string.h>

static uint32_t div_round_up(uint32_t value, uint32_t divisor)
{
    return value / divisor + (value % divisor != 0u);
}

static FfxFsr4DispatchSize dispatch_size(uint32_t x, uint32_t y)
{
    FfxFsr4DispatchSize result = {x, y, 1u};
    return result;
}

bool ffxFsr4BuildDot4Schedule(uint32_t outputWidth,
                             uint32_t outputHeight,
                             FfxFsr4Dot4Schedule *outSchedule)
{
    FfxFsr4Dot4Schedule schedule;
    uint32_t encoder0Width, encoder0Height;
    uint32_t encoder1Width, encoder1Height;
    uint32_t encoder2Width, encoder2Height;

    if (!outSchedule || !outputWidth || !outputHeight ||
        outputWidth > FFX_FSR4_DOT4_MAX_OUTPUT_WIDTH ||
        outputHeight > FFX_FSR4_DOT4_MAX_OUTPUT_HEIGHT)
        return false;

    memset(&schedule, 0, sizeof(schedule));
    schedule.alignedWidth = (outputWidth + 7u) & ~7u;
    schedule.alignedHeight = (outputHeight + 7u) & ~7u;

    encoder0Width = schedule.alignedWidth / 2u;
    encoder0Height = schedule.alignedHeight / 2u;
    encoder1Width = schedule.alignedWidth / 4u;
    encoder1Height = schedule.alignedHeight / 4u;
    encoder2Width = schedule.alignedWidth / 8u;
    encoder2Height = schedule.alignedHeight / 8u;

    schedule.pre = dispatch_size(div_round_up(schedule.alignedWidth, 16u),
                                 div_round_up(schedule.alignedHeight, 16u));

    schedule.model[0] = dispatch_size(div_round_up(encoder0Width, 64u), encoder0Height);
    schedule.model[1] = schedule.model[0];
    schedule.model[2] = dispatch_size(div_round_up(encoder1Width, 64u), encoder1Height);
    schedule.model[3] = schedule.model[2];
    schedule.model[4] = schedule.model[2];
    schedule.model[5] = dispatch_size(div_round_up(encoder2Width, 64u), encoder2Height);
    schedule.model[6] = schedule.model[5];
    schedule.model[7] = schedule.model[5];
    schedule.model[8] = dispatch_size(div_round_up(encoder2Width, 8u),
                                      div_round_up(encoder2Height, 8u));
    schedule.model[9] = dispatch_size(div_round_up(encoder1Width, 64u), encoder1Height);
    schedule.model[10] = schedule.model[9];
    schedule.model[11] = dispatch_size(div_round_up(encoder0Width, 64u), encoder0Height);

    schedule.post = dispatch_size(div_round_up(schedule.alignedWidth, 16u),
                                  div_round_up(schedule.alignedHeight, 16u));
    schedule.rcas = schedule.post;

    *outSchedule = schedule;
    return true;
}

bool ffxFsr4BuildSpdSchedule(uint32_t renderWidth,
                             uint32_t renderHeight,
                             FfxFsr4SpdSchedule *outSchedule)
{
    FfxFsr4SpdSchedule schedule;
    uint32_t maxDimension;

    if (!outSchedule || !renderWidth || !renderHeight)
        return false;

    memset(&schedule, 0, sizeof(schedule));
    schedule.dispatch = dispatch_size(div_round_up(renderWidth, 64u),
                                      div_round_up(renderHeight, 64u));
    if (schedule.dispatch.x > UINT32_MAX / schedule.dispatch.y)
        return false;
    schedule.workgroupCount = schedule.dispatch.x * schedule.dispatch.y;
    maxDimension = renderWidth > renderHeight ? renderWidth : renderHeight;
    while (maxDimension > 1u && schedule.mipCount < 12u) {
        maxDimension >>= 1u;
        ++schedule.mipCount;
    }

    *outSchedule = schedule;
    return true;
}

size_t ffxFsr4GetDot4ScratchSize(uint32_t maxOutputWidth,
                                uint32_t maxOutputHeight)
{
    if (!maxOutputWidth || !maxOutputHeight ||
        maxOutputWidth > FFX_FSR4_DOT4_MAX_OUTPUT_WIDTH ||
        maxOutputHeight > FFX_FSR4_DOT4_MAX_OUTPUT_HEIGHT)
        return 0u;
    if (maxOutputWidth <= 1920u && maxOutputHeight <= 1080u)
        return 20880256u;
    if (maxOutputWidth <= 3840u && maxOutputHeight <= 2160u)
        return 83232256u;
    return 332352256u;
}

FfxFsr4ModelPreset ffxFsr4SelectModelPreset(uint32_t renderWidth,
                                           uint32_t outputWidth,
                                           bool dynamicResolutionModel)
{
    float ratio;

    if (dynamicResolutionModel)
        return FFX_FSR4_MODEL_PRESET_DRS;
    if (!renderWidth)
        return FFX_FSR4_MODEL_PRESET_NATIVE_AA;

    ratio = (float)outputWidth / (float)renderWidth;
    if (ratio >= 2.99f)
        return FFX_FSR4_MODEL_PRESET_ULTRA_PERFORMANCE;
    if (ratio >= 1.99f)
        return FFX_FSR4_MODEL_PRESET_PERFORMANCE;
    if (ratio >= 1.69f)
        return FFX_FSR4_MODEL_PRESET_BALANCED;
    if (ratio >= 1.50f)
        return FFX_FSR4_MODEL_PRESET_QUALITY;
    return FFX_FSR4_MODEL_PRESET_NATIVE_AA;
}

const char *ffxFsr4ModelPresetName(FfxFsr4ModelPreset preset)
{
    switch (preset) {
    case FFX_FSR4_MODEL_PRESET_NATIVE_AA:         return "native";
    case FFX_FSR4_MODEL_PRESET_QUALITY:           return "quality";
    case FFX_FSR4_MODEL_PRESET_BALANCED:          return "balanced";
    case FFX_FSR4_MODEL_PRESET_PERFORMANCE:       return "performance";
    case FFX_FSR4_MODEL_PRESET_ULTRA_PERFORMANCE: return "ultraperf";
    case FFX_FSR4_MODEL_PRESET_DRS:               return "drs";
    default:                                      return "unknown";
    }
}

const char *ffxFsr4ModelVariantName(FfxFsr4ModelVariant variant)
{
    switch (variant) {
    case FFX_FSR4_VARIANT_V07_INT8:       return "v07_i8";
    case FFX_FSR4_VARIANT_412_NAVI2_INT8: return "412_i8_2";
    case FFX_FSR4_VARIANT_412_LITE_INT8:  return "412_lite_i8";
    case FFX_FSR4_VARIANT_412_RDNA3_INT8: return "412_i8";
    default:                              return "unknown";
    }
}

size_t ffxFsr4GetVariantScratchSize(FfxFsr4ModelVariant variant,
                                    uint32_t maxOutputWidth,
                                    uint32_t maxOutputHeight)
{
    if (!maxOutputWidth || !maxOutputHeight ||
        maxOutputWidth > FFX_FSR4_DOT4_MAX_OUTPUT_WIDTH ||
        maxOutputHeight > FFX_FSR4_DOT4_MAX_OUTPUT_HEIGHT)
        return 0u;

    if (variant == FFX_FSR4_VARIANT_412_LITE_INT8) {
        if (maxOutputWidth <= 1920u && maxOutputHeight <= 1080u)
            return 25027392u;
        if (maxOutputWidth <= 3840u && maxOutputHeight <= 2160u)
            return 99820992u;
        return 398707392u;
    }

    return ffxFsr4GetDot4ScratchSize(maxOutputWidth, maxOutputHeight);
}

static FfxFsr4DispatchSize post_pad_dispatch(uint32_t tw, uint32_t th,
                                             uint32_t mw, uint32_t mh,
                                             uint32_t pad)
{
    int32_t diff_h = (int32_t)(mh + pad) - (int32_t)th;
    int32_t diff_w = (int32_t)(mw + pad) - (int32_t)tw;
    uint32_t dh = (uint32_t)((diff_h > 1 ? 1 : diff_h) + (int32_t)pad);
    uint32_t dw = (uint32_t)((diff_w > 5 ? 5 : diff_w) + (int32_t)pad);
    uint32_t border = (tw + dw) * (th + dh) - tw * th;
    return dispatch_size(div_round_up(border, 32u), 1u);
}

bool ffxFsr4Build412Schedule(FfxFsr4ModelVariant variant,
                             uint32_t outputWidth,
                             uint32_t outputHeight,
                             uint32_t maxOutputWidth,
                             uint32_t maxOutputHeight,
                             FfxFsr4412Schedule *outSchedule)
{
    FfxFsr4412Schedule schedule;
    uint32_t tierMaxW, tierMaxH;

    if (!outSchedule || !outputWidth || !outputHeight ||
        !maxOutputWidth || !maxOutputHeight ||
        outputWidth > maxOutputWidth || outputHeight > maxOutputHeight ||
        maxOutputWidth > FFX_FSR4_DOT4_MAX_OUTPUT_WIDTH ||
        maxOutputHeight > FFX_FSR4_DOT4_MAX_OUTPUT_HEIGHT)
        return false;

    if (maxOutputWidth > 3840u || maxOutputHeight > 2160u) {
        tierMaxW = 7680u;
        tierMaxH = 4320u;
    } else if (maxOutputWidth > 1920u || maxOutputHeight > 1080u) {
        tierMaxW = 3840u;
        tierMaxH = 2160u;
    } else {
        tierMaxW = 1920u;
        tierMaxH = 1080u;
    }

    memset(&schedule, 0, sizeof(schedule));
    schedule.alignedWidth = (outputWidth + 7u) & ~7u;
    schedule.alignedHeight = (outputHeight + 7u) & ~7u;

    if (variant == FFX_FSR4_VARIANT_412_LITE_INT8) {
        static const uint32_t denoms[FFX_FSR4_412_LITE_TENSORS] = {
            2u, 1u, 2u, 4u, 4u, 2u, 1u, 1u, 1u
        };
        static const uint32_t pads[FFX_FSR4_412_LITE_TENSORS] = {
            1u, 0u, 1u, 1u, 1u, 1u, 0u, 0u, 0u
        };
        schedule.tensorCount = FFX_FSR4_412_LITE_TENSORS;
        schedule.modelPassCount = FFX_FSR4_412_LITE_MODEL_PASS_COUNT;
        for (uint32_t i = 0; i < FFX_FSR4_412_LITE_TENSORS; ++i) {
            schedule.tensorSizes[i][0] = schedule.alignedWidth / denoms[i];
            schedule.tensorSizes[i][1] = schedule.alignedHeight / denoms[i];
            schedule.tensorSizes[i][2] = 0u;
            schedule.tensorSizes[i][3] = 0u;
        }
        schedule.pre = dispatch_size(
            div_round_up(schedule.tensorSizes[0][0], 8u),
            div_round_up(schedule.tensorSizes[0][1], 8u));
        schedule.model[0] = post_pad_dispatch(
            schedule.tensorSizes[0][0], schedule.tensorSizes[0][1],
            tierMaxW / denoms[0], tierMaxH / denoms[0], pads[0]);
        schedule.model[1] = dispatch_size(
            div_round_up(schedule.tensorSizes[2][0], 32u),
            div_round_up(schedule.tensorSizes[2][1], 4u));
        schedule.model[2] = post_pad_dispatch(
            schedule.tensorSizes[2][0], schedule.tensorSizes[2][1],
            tierMaxW / denoms[2], tierMaxH / denoms[2], pads[2]);
        schedule.model[3] = post_pad_dispatch(
            schedule.tensorSizes[3][0], schedule.tensorSizes[3][1],
            tierMaxW / denoms[3], tierMaxH / denoms[3], pads[3]);
        schedule.model[4] = dispatch_size(
            div_round_up(schedule.tensorSizes[4][0], 64u),
            schedule.tensorSizes[4][1]);
        schedule.model[5] = post_pad_dispatch(
            schedule.tensorSizes[4][0], schedule.tensorSizes[4][1],
            tierMaxW / denoms[4], tierMaxH / denoms[4], pads[4]);
        schedule.model[6] = dispatch_size(
            div_round_up(schedule.tensorSizes[5][0], 128u),
            div_round_up(schedule.tensorSizes[5][1], 2u));
        schedule.model[7] = post_pad_dispatch(
            schedule.tensorSizes[5][0], schedule.tensorSizes[5][1],
            tierMaxW / denoms[5], tierMaxH / denoms[5], pads[5]);
        schedule.post = dispatch_size(
            div_round_up(schedule.tensorSizes[6][0], 32u),
            div_round_up(schedule.tensorSizes[6][1], 32u));
        schedule.rcas = dispatch_size(
            div_round_up(schedule.alignedWidth, 16u),
            div_round_up(schedule.alignedHeight, 16u));
        *outSchedule = schedule;
        return true;
    }

    if (variant == FFX_FSR4_VARIANT_412_NAVI2_INT8 ||
        variant == FFX_FSR4_VARIANT_412_RDNA3_INT8) {
        static const uint32_t denoms[FFX_FSR4_412_MAX_TENSORS] = {
            2u, 1u, 2u, 2u, 4u, 4u, 4u, 8u, 8u, 8u, 4u, 4u, 2u, 2u, 1u, 1u, 1u
        };
        static const uint32_t pads[FFX_FSR4_412_MAX_TENSORS] = {
            1u, 0u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 1u, 0u, 0u, 0u
        };
        schedule.tensorCount = FFX_FSR4_412_MAX_TENSORS;
        schedule.modelPassCount = FFX_FSR4_412_MODEL_PASS_COUNT;
        for (uint32_t i = 0; i < FFX_FSR4_412_MAX_TENSORS; ++i) {
            schedule.tensorSizes[i][0] = schedule.alignedWidth / denoms[i];
            schedule.tensorSizes[i][1] = schedule.alignedHeight / denoms[i];
            schedule.tensorSizes[i][2] = 0u;
            schedule.tensorSizes[i][3] = 0u;
        }
        schedule.pre = dispatch_size(
            div_round_up(schedule.tensorSizes[0][0], 8u),
            div_round_up(schedule.tensorSizes[0][1], 8u));
        schedule.model[0] = post_pad_dispatch(
            schedule.tensorSizes[0][0], schedule.tensorSizes[0][1],
            tierMaxW / denoms[0], tierMaxH / denoms[0], pads[0]);
        for (uint32_t t = 2u; t <= 13u; ++t) {
            uint32_t m_idx = (t - 2u) * 2u + 1u;
            if ((t == 10u || t == 12u) &&
                variant == FFX_FSR4_VARIANT_412_NAVI2_INT8) {
                schedule.model[m_idx] = dispatch_size(
                    div_round_up(schedule.tensorSizes[t][0], 16u),
                    div_round_up(schedule.tensorSizes[t][1], 8u));
            } else if (t == 10u || t == 12u) {
                schedule.model[m_idx] = dispatch_size(
                    div_round_up(schedule.tensorSizes[t][0], 128u),
                    div_round_up(schedule.tensorSizes[t][1], 2u));
            } else {
                schedule.model[m_idx] = dispatch_size(
                    div_round_up(schedule.tensorSizes[t][0], 64u),
                    schedule.tensorSizes[t][1]);
            }
            schedule.model[m_idx + 1u] = post_pad_dispatch(
                schedule.tensorSizes[t][0], schedule.tensorSizes[t][1],
                tierMaxW / denoms[t], tierMaxH / denoms[t], pads[t]);
        }
        schedule.post = dispatch_size(
            div_round_up(schedule.tensorSizes[14][0], 32u),
            div_round_up(schedule.tensorSizes[14][1], 32u));
        schedule.rcas = dispatch_size(
            div_round_up(schedule.alignedWidth, 16u),
            div_round_up(schedule.alignedHeight, 16u));
        *outSchedule = schedule;
        return true;
    }

    return false;
}
