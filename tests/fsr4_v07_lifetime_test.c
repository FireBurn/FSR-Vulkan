/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 */

#include "ffx_vk_fsr4_v07.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
        result = 1; \
        goto cleanup; \
    } \
} while (0)

static int choose_compute_device(VkInstance instance, VkPhysicalDevice *physical,
                                 uint32_t *queue_family)
{
    uint32_t count = 0;
    VkPhysicalDevice *devices = NULL;
    int found = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, NULL) != VK_SUCCESS || !count)
        return 0;
    devices = calloc(count, sizeof(*devices));
    if (!devices || vkEnumeratePhysicalDevices(instance, &count, devices) != VK_SUCCESS)
        goto done;
    for (uint32_t d = 0; d < count && !found; ++d) {
        uint32_t queue_count = 0;
        VkQueueFamilyProperties *queues = NULL;
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(devices[d], &properties);
        if (VK_API_VERSION_MAJOR(properties.apiVersion) < 1 ||
            (VK_API_VERSION_MAJOR(properties.apiVersion) == 1 &&
             VK_API_VERSION_MINOR(properties.apiVersion) < 3))
            continue;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queue_count, NULL);
        queues = calloc(queue_count, sizeof(*queues));
        if (!queues)
            continue;
        vkGetPhysicalDeviceQueueFamilyProperties(devices[d], &queue_count, queues);
        for (uint32_t q = 0; q < queue_count; ++q) {
            if (queues[q].queueCount &&
                (queues[q].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                *physical = devices[d];
                *queue_family = q;
                found = 1;
                break;
            }
        }
        free(queues);
    }
done:
    free(devices);
    return found;
}

int main(void)
{
    VkInstance instance = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t queue_family = UINT32_MAX;
    FfxInterface backend;
    void *scratch = NULL;
    int result = 0;

    memset(&backend, 0, sizeof(backend));
    VkApplicationInfo application = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .apiVersion = VK_API_VERSION_1_3,
    };
    VkInstanceCreateInfo instance_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application,
    };
    if (vkCreateInstance(&instance_info, NULL, &instance) != VK_SUCCESS ||
        !choose_compute_device(instance, &physical, &queue_family)) {
        result = 77;
        goto cleanup;
    }

    const float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    VkDeviceCreateInfo device_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
    };
    if (vkCreateDevice(physical, &device_info, NULL, &device) != VK_SUCCESS) {
        result = 77;
        goto cleanup;
    }

    scratch = calloc(1, ffxFsr4VkGetScratchMemorySize());
    CHECK(scratch);
    FfxFsr4VkCreateInfo create_info = {
        .device = device,
        .physicalDevice = physical,
        .scratchBuffer = scratch,
        .scratchBufferSize = ffxFsr4VkGetScratchMemorySize(),
    };
    const VkResult context_result = ffxFsr4VkCreateContext(&create_info, &backend);
    if (context_result == VK_ERROR_FEATURE_NOT_PRESENT ||
        context_result == VK_ERROR_FORMAT_NOT_SUPPORTED) {
        result = 77;
        goto cleanup;
    }
    CHECK(context_result == VK_SUCCESS);

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
    };
    CHECK(vkCreateCommandPool(device, &pool_info, NULL, &command_pool) == VK_SUCCESS);
    VkCommandBufferAllocateInfo command_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    CHECK(vkAllocateCommandBuffers(device, &command_info, &command_buffer) == VK_SUCCESS);

    /* Three unretired records are supported. A fourth must not recycle a
     * descriptor/constant partition without the host's completion signal. */
    for (uint64_t frame_id = 1; frame_id <= 3; ++frame_id) {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        CHECK(vkBeginCommandBuffer(command_buffer, &begin_info) == VK_SUCCESS);
        CHECK(ffxFsr4VkBeginFrame(&backend, frame_id) == VK_SUCCESS);
        CHECK(backend.fpExecuteGpuJobs(&backend, (FfxCommandList)command_buffer, 0) == FFX_OK);
        CHECK(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
        CHECK(vkResetCommandBuffer(command_buffer, 0) == VK_SUCCESS);
    }
    CHECK(ffxFsr4VkBeginFrame(&backend, 4) == VK_NOT_READY);
    CHECK(ffxFsr4VkRetireFrame(&backend, 1) == VK_SUCCESS);
    CHECK(ffxFsr4VkBeginFrame(&backend, 4) == VK_SUCCESS);
    CHECK(ffxFsr4VkRetireFrame(&backend, 4) == VK_SUCCESS);

cleanup:
    if (backend.device)
        ffxFsr4VkDestroyContext((FfxFsr4VkContext *)backend.device);
    if (command_pool)
        vkDestroyCommandPool(device, command_pool, NULL);
    if (device)
        vkDestroyDevice(device, NULL);
    if (instance)
        vkDestroyInstance(instance, NULL);
    free(scratch);
    return result;
}
