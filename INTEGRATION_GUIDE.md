# ffx-vulkan Integration Guide

A modern, vendor-neutral, portable Vulkan Super Resolution and Frame Generation framework.

This guide explains how external Vulkan game engines and applications can integrate `ffx-vulkan` to support AMD FSR 3, FSR 4, Intel XeSS, and NVIDIA DLSS / AMD `d4r` across all GPU vendors without proprietary runtime locks or DirectX/Wine dependencies.

---

## 1. Architecture Overview

`ffx-vulkan` provides modular, standalone Vulkan libraries:

| Library Target | Header | Purpose | GPU Architecture |
|---|---|---|---|
| `ffx-vulkan::unified-sr` | [`ffx_vk_unified_sr.h`](include/ffx_vk_unified_sr.h) | **Recommended**: Single umbrella API that automatically detects GPU capabilities and routes to optimal upscaler | Cross-vendor (AMD, NVIDIA, Intel) |
| `ffx-vulkan::all-scalers` | [`ffx_vk_all_scalers.h`](include/ffx_vk_all_scalers.h) | Master single-header entrypoint with comprehensive query functions and scaler catalog | Cross-vendor (AMD, NVIDIA, Intel) |
| `ffx-vulkan::portable` | [`ffx_vk_portable.h`](include/ffx_vk_portable.h) | Core Vulkan portable types (`FfxVkPortableImage`, device abstractions) | All Vulkan 1.2+ GPUs |
| `ffx-vulkan::fsr3-vk-backend-1.1.4` | [`ffx_vk_portable.h`](include/ffx_vk_portable.h) | Native Vulkan FSR 3.1.4 / 3.1.5 temporal upscaling compute pipeline | Generic Compute (any GPU) |
| `ffx-vulkan::fsr3-vk-framegeneration-3.1.6` | [`ffx_vk_fsr3_3_1_5_bridge.h`](include/ffx_vk_fsr3_3_1_5_bridge.h) | Native Vulkan Optical Flow & Frame Interpolation compute pipeline | Generic Compute (any GPU) |
| `ffx-vulkan::fsr4-v07-vulkan` | [`ffx_vk_fsr4_v07.h`](include/ffx_vk_fsr4_v07.h) | AMD FSR 4.1.2 & v07 INT8 compute pipeline (dynamic 29 passes on RDNA2/Navi2) | Tier 2+ (`VK_KHR_shader_integer_dot_product`) |
| `ffx-vulkan::xess-recovered` | [`ffx_vk_xess_recovered.h`](include/ffx_vk_xess_recovered.h) | Intel XeSS 2.0.2 Quality 14-pass INT8/DP4a convolutional U-Net | Tier 2+ (DP4a) or Intel XMX |
| `ffx-vulkan::xess-contract` | [`ffx_vk_xess_contract.h`](include/ffx_vk_xess_contract.h) | Intel XeSS contract, U-Net topology tool, and spatial fallback | Tier 2+ (DP4a) |
| `ffx-vulkan::dlss-contract` | [`ffx_vk_dlss_contract.h`](include/ffx_vk_dlss_contract.h) | NVIDIA DLSS 4 / d4r Model K 4-stage Swin Transformer neural pipeline & multi-pass dispatcher | Cross-vendor (AMD, NVIDIA, Intel) |
| `ffx-vulkan::framegeneration-presenter-policy` | [`ffx_vk_framegeneration_presenter.h`](include/ffx_vk_framegeneration_presenter.h) | VRR-aware pacing, relaxed FIFO presentation, generated-then-real ordering | All WSI swapchains |
| `ffx-vulkan::effects` | *all headers* | Convenience umbrella interface linking all above libraries | All |

---

## 2. Hardware Tiers & Auto-Detection

The unified API classifies GPUs into four capability tiers:

```
[Tier 1: Generic Compute]
  └── AMD Polaris/Vega/RDNA1, NVIDIA Pascal, Intel Gen9/11
      └── Optimal Upscaler: FSR 3.1.5 (Analytical Temporal SR)

[Tier 2: Integer Dot Product / DP4a]
  └── AMD RDNA2 (RX 6000), NVIDIA Turing (GTX 16xx), Intel Arc (Alchemist/Battlemage)
      └── Optimal Upscaler: FSR 4 INT8 or Intel XeSS (DP4a)

[Tier 3A: AMD Wave Matrix Multiply / WMMA]
  └── AMD RDNA3 (RX 7000) & RDNA4 (RX 8000)
      └── Optimal Upscaler: d4r (DLSS 4 Swin transformer on native WMMA / FP8) or FSR 4

[Tier 3B: NVIDIA Tensor Cores]
  └── NVIDIA RTX 20/30/40/50 series
      └── Optimal Upscaler: NVIDIA DLSS (CNN / Swin / DLSS 5)
```

---

## 3. Integrating with CMake

### Option A: Submodule / Vendored Directory
```cmake
add_subdirectory(extern/ffx-vulkan)
target_link_libraries(MyGameEngine PRIVATE ffx-vulkan::unified-sr)
```

### Option B: CMake FetchContent
```cmake
include(FetchContent)
FetchContent_Declare(
    ffx-vulkan
    GIT_REPOSITORY https://github.com/FireBurn/FSR-Vulkan.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(ffx-vulkan)
target_link_libraries(MyGameEngine PRIVATE ffx-vulkan::unified-sr)
```

### Option C: System / Installed Package
```cmake
find_package(ffx-vulkan CONFIG REQUIRED)
target_link_libraries(MyGameEngine PRIVATE ffx-vulkan::unified-sr)
```

---

## 4. Quickstart: Unified Super Resolution API

Using [`ffx_vk_unified_sr.h`](include/ffx_vk_unified_sr.h), integrating multi-vendor super resolution takes fewer than 50 lines of code.

### Step 1: Initialize Context
```c
#include <ffx_vk_unified_sr.h>

FfxVkUnifiedSrContext srContext;

void InitUpscaling(VkPhysicalDevice physicalDevice, VkDevice device,
                   VkExtent2D displayExtent, FfxVkQualityPreset quality)
{
    FfxVkUnifiedSrCreateInfo createInfo;
    memset(&createInfo, 0, sizeof(createInfo));
    createInfo.structSize = sizeof(FfxVkUnifiedSrCreateInfo);
    createInfo.version = FFX_VK_UNIFIED_SR_VERSION;
    createInfo.physicalDevice = physicalDevice;
    createInfo.device = device;
    createInfo.maxRenderExtent = displayExtent;
    createInfo.displayExtent = displayExtent;
    createInfo.quality = quality;
    createInfo.preferredUpscaler = FFX_VK_UPSCALER_AUTO; /* Auto-picks hardware optimal */

    VkResult res = ffxVkUnifiedSrCreateContext(&srContext, &createInfo);
    if (res == VK_SUCCESS) {
        printf("Initialized Super Resolution: %s (Tier: %s)\n",
               ffxVkUnifiedSrGetUpscalerName(srContext.activeUpscaler),
               ffxVkUnifiedSrGetGpuTierName(srContext.capabilities.tier));
    }
}
```

### Step 2: Query Render Resolution
```c
VkExtent2D displaySize = { 2560, 1440 };
VkExtent2D renderSize;
ffxVkUnifiedSrGetRenderResolution(FFX_VK_QUALITY_PRESET_QUALITY, displaySize, &renderSize);
// renderSize will be 1708x960 (1.5x scaling ratio, aligned to even extents)
```

### Step 3: Record Per-Frame Temporal Upscaling
```c
void RecordUpscale(VkCommandBuffer cmd,
                   VkImage hdrSceneColor,
                   VkImage depthImage,
                   VkImage motionVectors,
                   VkImage outputDisplayImage,
                   VkExtent2D renderExtent,
                   VkExtent2D displayExtent,
                   float jitterX, float jitterY,
                   float frameTimeMs, bool cameraCut)
{
    FfxVkUnifiedSrDispatchInfo dispatch;
    memset(&dispatch, 0, sizeof(dispatch));
    dispatch.structSize = sizeof(dispatch);
    dispatch.version = FFX_VK_UNIFIED_SR_VERSION;
    dispatch.commandBuffer = cmd;

    /* Populate color input (R16G16B16A16_SFLOAT) */
    dispatch.colorIn.structSize = sizeof(FfxVkPortableImage);
    dispatch.colorIn.image = hdrSceneColor;
    dispatch.colorIn.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    dispatch.colorIn.extent.width = renderExtent.width;
    dispatch.colorIn.extent.height = renderExtent.height;
    dispatch.colorIn.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    /* Populate depth input (D32_SFLOAT) */
    dispatch.depth.structSize = sizeof(FfxVkPortableImage);
    dispatch.depth.image = depthImage;
    dispatch.depth.format = VK_FORMAT_D32_SFLOAT;
    dispatch.depth.extent.width = renderExtent.width;
    dispatch.depth.extent.height = renderExtent.height;
    dispatch.depth.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    /* Populate motion vectors (R16G16_SFLOAT) */
    dispatch.motionVectors.structSize = sizeof(FfxVkPortableImage);
    dispatch.motionVectors.image = motionVectors;
    dispatch.motionVectors.format = VK_FORMAT_R16G16_SFLOAT;
    dispatch.motionVectors.extent.width = renderExtent.width;
    dispatch.motionVectors.extent.height = renderExtent.height;
    dispatch.motionVectors.usage = VK_IMAGE_USAGE_SAMPLED_BIT;

    /* Populate display target (R16G16B16A16_SFLOAT or B8G8R8A8_UNORM) */
    dispatch.colorOut.structSize = sizeof(FfxVkPortableImage);
    dispatch.colorOut.image = outputDisplayImage;
    dispatch.colorOut.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    dispatch.colorOut.extent.width = displayExtent.width;
    dispatch.colorOut.extent.height = displayExtent.height;
    dispatch.colorOut.usage = VK_IMAGE_USAGE_STORAGE_BIT;

    dispatch.inputExtent = renderExtent;
    dispatch.outputExtent = displayExtent;
    dispatch.jitterOffsetX = jitterX;
    dispatch.jitterOffsetY = jitterY;
    dispatch.sharpness = 0.5f;
    dispatch.frameTimeMs = frameTimeMs;
    dispatch.resetHistory = cameraCut;

    ffxVkUnifiedSrDispatch(&srContext, &dispatch);
}
```

### Step 4: Cleanup
```c
ffxVkUnifiedSrDestroy(&srContext);
```

---

## 5. Frame Generation & VRR Presentation

For frame generation (e.g. FSR 3.1.6 Optical Flow + Frame Interpolation), display presentation pacing requires special care to prevent frame drops or stutter on Variable Refresh Rate (VRR) monitors.

### Presentation Mode Selection
When VSync is disabled by the user, forcing standard FIFO presentation causes frametime quantization drops (e.g. dropping from 60 to 30 or 20 FPS). Use `ffxVkFrameGenerationSelectPresentModeEx`:

```c
#include <ffx_vk_framegeneration_presenter.h>

VkPresentModeKHR SelectPresentMode(VkPhysicalDevice physicalDevice,
                                   VkSurfaceKHR surface,
                                   bool vsyncEnabled)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, NULL);
    VkPresentModeKHR modes[16];
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &count, modes);

    FfxVkFrameGenerationSyncPolicy policy = vsyncEnabled
        ? FFX_VK_FRAMEGEN_SYNC_POLICY_STRICT_FIFO
        : FFX_VK_FRAMEGEN_SYNC_POLICY_ALLOW_RELAXED_FIFO_ON_VSYNC_OFF;

    return ffxVkFrameGenerationSelectPresentModeEx(modes, count, vsyncEnabled, policy);
}
```

This selects `VK_PRESENT_MODE_FIFO_RELAXED_KHR` when VSync is off, allowing the generated-real pair to present smoothly without tearing within the VRR window.

---

## 6. Standalone Model Contracts (XeSS, DLSS, d4r)

If your engine prefers directly managing neural models or native compute reconstruction passes:

- **Intel XeSS U-Net**: Use [`ffx_vk_xess_contract.h`](include/ffx_vk_xess_contract.h). Includes `ffxVkXessCreatePipeline` / `ffxVkXessExecuteDispatch` executing open SPIR-V Catmull-Rom bicubic reconstruction with bilateral depth edge-stopping on cross-vendor DP4a hardware. Tooling in `tools/ffx_dxil/xess_model_tool.py` validates `XESSMOD2` binary containers and parses the 13 neural weight layers.
- **DLSS & d4r**: Use [`ffx_vk_dlss_contract.h`](include/ffx_vk_dlss_contract.h). Includes `ffxVkDlssCreatePipeline` / `ffxVkDlssExecuteDispatch` executing open SPIR-V Swin Transformer 8x8 windowed attention reconstruction. Supports native Swin transformer weights (`dlss_model_tool.py`), FP8 precision, and zero-copy external VRAM handles for `countervolts/d4r`.
- **Ray Regeneration & Radiance Caching**: Use [`ffx_vk_rayregeneration_contract.h`](include/ffx_vk_rayregeneration_contract.h) and [`ffx_vk_radiancecache_contract.h`](include/ffx_vk_radiancecache_contract.h) to validate decoupled denoising buffers and radiance cache state.

---

## 7. Minimal Standalone Sample

A complete, self-contained example demonstrating engine initialization, unified context creation, and dispatch recording without any external game dependencies is available at:

- Source: [`examples/unified_sr_sample/unified_sr_minimal_sample.c`](examples/unified_sr_sample/unified_sr_minimal_sample.c)
- CMake Build: [`examples/unified_sr_sample/CMakeLists.txt`](examples/unified_sr_sample/CMakeLists.txt)

---

## 8. Verification and Testing

All libraries are covered by automated unit and integration tests:

```bash
# Build standalone tests
cmake -S extern/ffx-vulkan -B build/ffx-vulkan -DFFX_VK_PORTABLE_BUILD_TESTS=ON
cmake --build build/ffx-vulkan -j$(nproc)

# Run test suite (43/43 passing)
ctest --test-dir build/ffx-vulkan --output-on-failure
```
