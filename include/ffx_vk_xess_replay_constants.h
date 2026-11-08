/* SPDX-License-Identifier: MIT
 * Experimental recovered XeSS 2.0.2 cross-vendor profile, not the public
 * spatial-prototype backend. Captured quality extents only, exposure 1, zero
 * resource origins and the recovered model. No Vulkan dependency.
 */
#ifndef FFX_VK_XESS_REPLAY_CONSTANTS_H
#define FFX_VK_XESS_REPLAY_CONSTANTS_H
#include <stdint.h>
#include <string.h>
#include <math.h>

typedef struct FfxVkXeSSReplayFrame {
    float jitterX, jitterY;
    float previousJitterX, previousJitterY;
    uint32_t reset;
} FfxVkXeSSReplayFrame;

typedef struct FfxVkXeSSReplayExtent {
    uint32_t inputWidth, inputHeight, outputWidth, outputHeight;
} FfxVkXeSSReplayExtent;

/* One shared source of exact quality-profile input sizes for engine adapters.
 * Unknown outputs clear the result; no device/model availability is implied.
 */
static inline int ffxVkXeSSReplayExtentForOutput(uint32_t width, uint32_t height,
    FfxVkXeSSReplayExtent *extent)
{
    static const FfxVkXeSSReplayExtent profiles[] = {
        {565,318,960,540}, {753,424,1280,720}, {1130,636,1920,1080}
    };
    if (!extent) return 0;
    memset(extent,0,sizeof(*extent));
    for (size_t i=0;i<sizeof(profiles)/sizeof(profiles[0]);++i)
        if (profiles[i].outputWidth==width && profiles[i].outputHeight==height) {
            *extent=profiles[i];return 1;
        }
    return 0;
}

/* Derives input resolution for any display resolution and quality preset.
 * Preset values: 0 = Native AA (1.0x), 1 = Quality (1.7x), 2 = Balanced (2.0x),
 * 3 = Performance (2.3x), 4 = Ultra Performance (3.0x).
 */
static inline int ffxVkXeSSReplayExtentForOutputAndQuality(uint32_t width, uint32_t height,
    int quality, FfxVkXeSSReplayExtent *extent)
{
    if (!extent || width == 0 || height == 0) return 0;
    if ((width & 1u) || (height & 1u)) {
        memset(extent, 0, sizeof(*extent));
        return 0;
    }
    memset(extent, 0, sizeof(*extent));
    /* Exact reference captures for Quality mode */
    if (quality == 1) {
        if (width == 960 && height == 540) { *extent = (FfxVkXeSSReplayExtent){565, 318, 960, 540}; return 1; }
        if (width == 1280 && height == 720) { *extent = (FfxVkXeSSReplayExtent){753, 424, 1280, 720}; return 1; }
        if (width == 1920 && height == 1080) { *extent = (FfxVkXeSSReplayExtent){1130, 636, 1920, 1080}; return 1; }
    }
    float scale = 1.7f;
    switch (quality) {
    case 0: /* Native AA */ scale = 1.0f; break;
    case 1: /* Quality */ scale = 1.7f; break;
    case 2: /* Balanced */ scale = 2.0f; break;
    case 3: /* Performance */ scale = 2.3f; break;
    case 4: /* Ultra Performance */ scale = 3.0f; break;
    default: scale = 1.7f; break;
    }
    uint32_t inW = (uint32_t)ceilf((float)width / scale);
    uint32_t inH = (uint32_t)ceilf((float)height / scale);
    if (inW > width) inW = width;
    if (inH > height) inH = height;
    if (inW < 32) inW = 32;
    if (inH < 32) inH = 32;
    extent->inputWidth = inW;
    extent->inputHeight = inH;
    extent->outputWidth = width;
    extent->outputHeight = height;
    return 1;
}

/* Validates that extent dimensions are positive and input fits within output.
 * Preserves verification compatibility with captured profiles while enabling
 * derived multi-preset and custom resolution playback.
 */
static inline int ffxVkXeSSReplayExtentSupported(const FfxVkXeSSReplayExtent *extent)
{
    if (!extent) return 0;
    if (extent->inputWidth == 0 || extent->inputHeight == 0) return 0;
    if (extent->outputWidth == 0 || extent->outputHeight == 0) return 0;
    if (extent->inputWidth > extent->outputWidth || extent->inputHeight > extent->outputHeight) return 0;
    if (extent->outputWidth < 64 || extent->outputHeight < 64) return 0;
    if (extent->inputWidth < 32 || extent->inputHeight < 32) return 0;
    return 1;
}

static inline uint32_t ffxVkXeSSReplayFloat(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

/* Arrays include the normalized descriptor-table bases. Returns zero for
 * invalid input without modifying either output. Unidentified static fields
 * deliberately retain their captured values; this is a fixed quality profile.
 */
static inline int ffxVkXeSSReplayConstantsForExtent(const FfxVkXeSSReplayExtent *extent,
    const FfxVkXeSSReplayFrame *frame,
    uint32_t prepare[30], uint32_t resolve[23])
{
    if (!ffxVkXeSSReplayExtentSupported(extent) || !frame || !prepare || !resolve || frame->reset > 1 ||
        !isfinite(frame->jitterX) || !isfinite(frame->jitterY) ||
        !isfinite(frame->previousJitterX) || !isfinite(frame->previousJitterY) ||
        fabsf(frame->jitterX) > 0.5f || fabsf(frame->jitterY) > 0.5f ||
        fabsf(frame->previousJitterX) > 0.5f || fabsf(frame->previousJitterY) > 0.5f)
        return 0;
    memset(prepare, 0, 30 * sizeof(uint32_t));
    memset(resolve, 0, 23 * sizeof(uint32_t));
    prepare[0] = ffxVkXeSSReplayFloat((float)extent->inputWidth);
    prepare[1] = ffxVkXeSSReplayFloat((float)extent->inputHeight);
    prepare[2] = ffxVkXeSSReplayFloat(1.f / (float)extent->inputWidth);
    prepare[3] = ffxVkXeSSReplayFloat(1.f / (float)extent->inputHeight);
    prepare[4] = resolve[4] = ffxVkXeSSReplayFloat(frame->jitterX);
    prepare[5] = resolve[5] = ffxVkXeSSReplayFloat(frame->jitterY);
    prepare[6] = resolve[8] = ffxVkXeSSReplayFloat((float)extent->outputWidth / (float)extent->inputWidth);
    prepare[7] = resolve[9] = ffxVkXeSSReplayFloat((float)extent->outputHeight / (float)extent->inputHeight);
    prepare[8] = ffxVkXeSSReplayFloat((float)extent->outputWidth);
    prepare[9] = ffxVkXeSSReplayFloat((float)extent->outputHeight);
    prepare[10] = ffxVkXeSSReplayFloat(1.f / (float)extent->outputWidth);
    prepare[11] = ffxVkXeSSReplayFloat(1.f / (float)extent->outputHeight);
    prepare[20] = prepare[21] = prepare[24] = resolve[10] = 0x3f800000u;
    prepare[22] = ffxVkXeSSReplayFloat(frame->previousJitterX);
    prepare[23] = ffxVkXeSSReplayFloat(frame->previousJitterY);
    prepare[25] = resolve[13] = frame->reset;
    prepare[29] = 8;
    resolve[2] = extent->outputWidth - 1; resolve[3] = extent->outputHeight - 1;
    resolve[11] = extent->inputWidth; resolve[12] = extent->inputHeight;
    resolve[16] = 0x3f4ccccdu;
    resolve[21] = 4; resolve[22] = 9;
    return 1;
}
static inline int ffxVkXeSSReplayConstants(const FfxVkXeSSReplayFrame *frame,
    uint32_t prepare[30], uint32_t resolve[23])
{
    const FfxVkXeSSReplayExtent extent = {753,424,1280,720};
    return ffxVkXeSSReplayConstantsForExtent(&extent,frame,prepare,resolve);
}
#endif
