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
/* Only reference-verified quality-profile extents are enabled. */
static inline int ffxVkXeSSReplayExtentSupported(const FfxVkXeSSReplayExtent *extent)
{
    FfxVkXeSSReplayExtent expected;
    return extent && ffxVkXeSSReplayExtentForOutput(extent->outputWidth,extent->outputHeight,&expected) &&
        extent->inputWidth==expected.inputWidth && extent->inputHeight==expected.inputHeight;
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
