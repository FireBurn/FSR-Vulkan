/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_fsr3_3_1_5_reflection.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SPIRV_MAGIC 0x07230203u
#define SPIRV_OP_NAME 5u
#define SPIRV_OP_DECORATE 71u
#define SPIRV_DECORATION_BINDING 33u
#define SPIRV_DECORATION_DESCRIPTOR_SET 34u
#define NO_BINDING UINT_MAX
#define NO_DESCRIPTOR_SET UINT_MAX
#define MAX_SPIRV_BOUND (1u << 20)

static int descriptor_class(const char* name, FfxVkFsr3_3_1_5DescriptorClass* outClass)
{
    if (!name || !outClass)
        return 0;
    if (strncmp(name, "rw_", 3u) == 0) {
        *outClass = FFX_VK_FSR3_3_1_5_DESCRIPTOR_UAV;
        return 1;
    }
    if (strncmp(name, "r_", 2u) == 0) {
        *outClass = FFX_VK_FSR3_3_1_5_DESCRIPTOR_SRV;
        return 1;
    }
    if (strncmp(name, "s_", 2u) == 0) {
        *outClass = FFX_VK_FSR3_3_1_5_DESCRIPTOR_SAMPLER;
        return 1;
    }
    if (strncmp(name, "cb", 2u) == 0) {
        *outClass = FFX_VK_FSR3_3_1_5_DESCRIPTOR_CONSTANT_BUFFER;
        return 1;
    }
    return 0;
}

static void copy_name(char destination[64], const char* source, uint32_t wordCount)
{
    const size_t available = (size_t)wordCount * sizeof(uint32_t);
    size_t count = 0;
    if (!source) {
        destination[0] = '\0';
        return;
    }
    while (count < available && count < 63u && source[count] != '\0')
        ++count;
    memcpy(destination, source, count);
    destination[count] = '\0';
}

static int compare_binding(const void* left, const void* right)
{
    const FfxVkFsr3_3_1_5DescriptorBinding* a = left;
    const FfxVkFsr3_3_1_5DescriptorBinding* b = right;
    return (a->binding > b->binding) - (a->binding < b->binding);
}

FfxVkPortableResult ffxVkFsr3_3_1_5ReflectSpirv(
    const uint32_t* words,
    size_t wordCount,
    FfxVkFsr3_3_1_5DescriptorBinding* outBindings,
    uint32_t* inOutBindingCount)
{
    const char** names;
    uint32_t* bindings;
    uint32_t* descriptorSets;
    uint32_t bound;
    uint32_t count = 0;
    FfxVkFsr3_3_1_5DescriptorClass ignoredClass;
    uint8_t seenBindings[1024] = {};
    size_t offset;

    if (!words || !inOutBindingCount)
        return FFX_VK_PORTABLE_ERROR_INVALID_POINTER;
    if (wordCount < 5u || words[0] != SPIRV_MAGIC)
        return FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;
    bound = words[3];
    if (bound == 0u || bound > MAX_SPIRV_BOUND)
        return FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;

    names = calloc(bound, sizeof(*names));
    bindings = malloc((size_t)bound * sizeof(*bindings));
    descriptorSets = malloc((size_t)bound * sizeof(*descriptorSets));
    if (!names || !bindings || !descriptorSets) {
        free(names);
        free(bindings);
        free(descriptorSets);
        return FFX_VK_PORTABLE_ERROR_OUT_OF_MEMORY;
    }
    for (uint32_t index = 0; index < bound; ++index) {
        bindings[index] = NO_BINDING;
        descriptorSets[index] = NO_DESCRIPTOR_SET;
    }

    for (offset = 5u; offset < wordCount;) {
        const uint32_t instruction = words[offset];
        const uint32_t instructionWords = instruction >> 16u;
        const uint32_t opcode = instruction & 0xffffu;
        if (instructionWords == 0u || instructionWords > wordCount - offset) {
            free(names);
            free(bindings);
            free(descriptorSets);
            return FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;
        }
        if (opcode == SPIRV_OP_NAME && instructionWords >= 3u && words[offset + 1u] < bound) {
            names[words[offset + 1u]] = (const char*)&words[offset + 2u];
        } else if (opcode == SPIRV_OP_DECORATE && instructionWords >= 4u &&
                   words[offset + 1u] < bound &&
                   words[offset + 2u] == SPIRV_DECORATION_BINDING) {
            bindings[words[offset + 1u]] = words[offset + 3u];
        } else if (opcode == SPIRV_OP_DECORATE && instructionWords >= 4u &&
                   words[offset + 1u] < bound &&
                   words[offset + 2u] == SPIRV_DECORATION_DESCRIPTOR_SET) {
            descriptorSets[words[offset + 1u]] = words[offset + 3u];
        }
        offset += instructionWords;
    }

    for (uint32_t index = 0; index < bound; ++index) {
        if (bindings[index] != NO_BINDING) {
            if (descriptorSets[index] != 0u || bindings[index] >= 1024u ||
                !descriptor_class(names[index], &ignoredClass)) {
                free(names);
                free(bindings);
                free(descriptorSets);
                return FFX_VK_PORTABLE_ERROR_UNSUPPORTED;
            }
            if (seenBindings[bindings[index]]) {
                free(names);
                free(bindings);
                free(descriptorSets);
                return FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;
            }
            seenBindings[bindings[index]] = 1u;
            ++count;
        }
    }
    if (outBindings && *inOutBindingCount < count) {
        *inOutBindingCount = count;
        free(names);
        free(bindings);
        free(descriptorSets);
        return FFX_VK_PORTABLE_ERROR_INVALID_ARGUMENT;
    }
    if (outBindings) {
        uint32_t output = 0;
        for (uint32_t index = 0; index < bound; ++index) {
            if (bindings[index] == NO_BINDING)
                continue;
            outBindings[output].binding = bindings[index];
            if (!descriptor_class(names[index], &outBindings[output].descriptorClass)) {
                free(names);
                free(bindings);
                free(descriptorSets);
                return FFX_VK_PORTABLE_ERROR_UNSUPPORTED;
            }
            copy_name(outBindings[output].name, names[index],
                      names[index] ? (uint32_t)((wordCount - (size_t)(names[index] - (const char*)words) / sizeof(uint32_t))) : 0u);
            ++output;
        }
        qsort(outBindings, count, sizeof(*outBindings), compare_binding);
    }
    *inOutBindingCount = count;
    free(names);
    free(bindings);
    free(descriptorSets);
    return FFX_VK_PORTABLE_OK;
}
