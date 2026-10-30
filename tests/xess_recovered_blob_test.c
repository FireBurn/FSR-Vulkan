/* SPDX-License-Identifier: MIT
 * Container checks only: synthetic "abc" entries are not an executable model.
 */
#include "ffx_vk_xess_recovered.h"
#include <string.h>

enum { BLOB_BYTES = 2131 };
static unsigned char fixture[BLOB_BYTES + 1];
static void put32(unsigned char *out, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) out[i] = (unsigned char)(value >> (i * 8));
}
static void put64(unsigned char *out, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out[i] = (unsigned char)(value >> (i * 8));
}
static void reset(void) {
    memset(fixture, 0, sizeof(fixture)); memcpy(fixture, "XESSVK1\0", 8);
    put32(fixture + 8, 1); put32(fixture + 12, 53);
    for (unsigned i = 0; i < 53; ++i) {
        unsigned char *entry = fixture + 16 + i * 24;
        put64(entry, 1296 + i * 16); put64(entry + 8, 3);
        put32(entry + 16, 0x352441c2u); /* independently known IEEE CRC32("abc") */
        memcpy(fixture + 1296 + i * 16, "abc", 3);
    }
}
static int rejected(size_t bytes) {
    FfxVkXeSSRecoveredModel model = {0}; model.structSize = 123;
    char reason[128] = {0};
    return ffxVkXeSSRecoveredModelFromBlob(fixture, bytes, &model, reason, sizeof(reason)) ==
        VK_ERROR_INITIALIZATION_FAILED && model.structSize == 0 &&
        model.prepareShader.data == NULL && model.resolveBiases.sizeBytes == 0 && reason[0];
}
int main(void) {
    reset(); FfxVkXeSSRecoveredModel model = {0};
    if (ffxVkXeSSRecoveredModelFromBlob(fixture, BLOB_BYTES, &model, NULL, 0) != VK_SUCCESS ||
        model.structSize != sizeof(model) || model.prepareShader.sizeBytes != 3 ||
        model.resolveBiases.data != fixture + 2128 || memcmp(model.prepareShader.data, "abc", 3))
        return 1;
    if (!rejected(15) || !rejected(BLOB_BYTES - 1) || !rejected(BLOB_BYTES + 1)) return 2;
    fixture[BLOB_BYTES - 1] ^= 1;
    if (!rejected(BLOB_BYTES)) return 3;
    reset(); put32(fixture + 12, 52);
    if (!rejected(BLOB_BYTES)) return 4;
    reset(); put64(fixture + 16, UINT64_MAX);
    if (!rejected(BLOB_BYTES)) return 5;
    reset(); put64(fixture + 16 + 24, 1296); /* overlapping second entry */
    if (!rejected(BLOB_BYTES)) return 6;
    reset(); put32(fixture + 16 + 20, 1);
    if (!rejected(BLOB_BYTES)) return 7;
    reset(); put64(fixture + 16 + 8, UINT64_MAX);
    if (!rejected(BLOB_BYTES)) return 8;
    return 0;
}
