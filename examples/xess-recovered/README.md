# Recovered XeSS fixed-profile context

`ffx_vk_xess_fixed_context.hpp` provides an experimental C++17 context for the
recovered XeSS 2.0.2 cross-vendor graph. The quality profile uses unit exposure
and zero resource origins. Enabled input/output extents are:

| Input | Output |
| --- | --- |
| 565×318 | 960×540 |
| 753×424 | 1280×720 |
| 1130×636 | 1920×1080 |

Other extents are rejected until reference comparisons establish support. Model payloads
are caller-supplied in `ffxvk::XeSSFixedModel`; this repository does not ship
the proprietary shaders or weights.

The context accepts model bytes in memory and a borrowed Vulkan device. It
owns its pipelines, descriptors, model buffers, intermediate tensors and
temporal history. The engine owns input/output images, command buffers and
submission. No runtime DLL, JSON parser, filesystem access, host image copying,
device creation or queue wait occurs inside the context.

```cpp
#include <ffx_vk_xess_fixed_context.hpp>

// Create once; model can be released after construction.
ffxvk::XeSSFixedContext scaler(physicalDevice, device, model);
// Or choose an enabled extent when creating the context:
// ffxvk::XeSSFixedContext scaler(physicalDevice, device, model,
//     FfxVkXeSSReplayExtent{1130,636,1920,1080});

// Each XeSSExternalImage supplies image, view, extent and format.
// Retire this binding slot's previous use before changing its descriptors.
scaler.bind({color, motion, depth, output});

// Begin recording outside a render pass. The engine transitions external
// images to GENERAL and synchronizes producer writes before this call.
FfxVkXeSSReplayFrame frame{jitterX, jitterY, previousJitterX, previousJitterY, 1};
VkResult result = scaler.record(commands, frame);
// Check result, add an output barrier for the consumer stage, then submit.
// Subsequent frames use reset=0 and provide the previous frame's jitter.
```

Inputs are linear HDR RGBA16F color, RG16F previous-minus-current motion in
input pixels, and R32F conventional device depth in [0,1]. Output is RGBA16F.
All external views are mip-0, single-layer 2D views. The complete enabled
feature/limit and synchronization contract is documented on the context class.
Unsupported extents/formats and aliased input/output image handles are rejected
before descriptor updates. A first frame requires reset. Invalid frame
parameters return an error before recording commands.

The caller submits recordings once and in order on the same queue. If a
recording is discarded, retire previous submissions, call `discardHistory()`
and record a reset frame. Retire all GPU uses before destroying the
context. Model and external image descriptors are trusted caller data; this
API does not verify shader semantics or image usage flags.

For multiple frames in flight, supply the slot count at construction:

```cpp
ffxvk::XeSSFixedContext scaler(physicalDevice, device, model,
    FfxVkXeSSReplayExtent{1130,636,1920,1080}, 2);
// Wait for this slot's fence before reusing its bindings/resources.
scaler.bind(imagesForThisFrame, frameSlot);
VkResult result = scaler.record(commands, frame, frameSlot);
```

Each slot owns independent prepare/resolve descriptor sets; pipelines, weights
and temporally ordered internal images are shared. Slot counts from 1 to 8 are
accepted; omitted arguments retain one slot. Recording frames on the CPU and
submitting them in order preserves history without an internal queue wait.
The caller retains each slot's images and retires its previous use before
rebinding that slot. Rebinding another slot may happen while prior frames are
queued. Recording and rebinding a context are not thread-safe.

Geometry is derived from the output extent: each downsampling step rounds up
independently, so odd intermediate dimensions retain the correct skip links.
The same recovered shader/model bundle is used at each enabled size. Changing
resolution requires recreating the context after retiring its prior use.

The reference harness adapts a verified local native bundle into these typed
model structures and supplies alternating image sets. It checks exact output,
invalid arguments, initial reset and recovery after a discarded recording.
This profile still needs broader scene/resolution coverage and Q2RTX integration.

C engines can include `ffx_vk_xess_recovered.h` and link the exported
`ffx-vulkan::xess-recovered` target. Enable C and CXX in the consumer's CMake
project so the C++ implementation's runtime is linked. The installed full-stack
example compiles its ABI checks as C. No C++ type or exception crosses the API.
Vulkan errors and bounded diagnostics are returned to the caller.

Pack a local validated native bundle into one model asset:

```sh
python3 tools/ffx_dxil/pack_xess_model.py BUNDLE_DIRECTORY model.xessvk
```

The C loading path borrows the file bytes until creation completes:

```c
#include <ffx_vk_xess_recovered.h>

VkResult create_scaler(VkPhysicalDevice physical, VkDevice device,
    const void *file_data, size_t file_bytes,
    FfxVkXeSSRecoveredContext **context, char *reason, size_t reason_bytes)
{
    FfxVkXeSSRecoveredModel model = {0};
    VkResult result = ffxVkXeSSRecoveredModelFromBlob(file_data, file_bytes,
        &model, reason, reason_bytes);
    if (result != VK_SUCCESS) return result;
    FfxVkXeSSRecoveredCreateInfo info = {0};
    info.structSize = sizeof(info);
    info.abiVersion = FFX_VK_XESS_RECOVERED_ABI_VERSION;
    info.physicalDevice = physical;
    info.device = device;
    if (!ffxVkXeSSRecoveredGetExtent((VkExtent2D){1920,1080}, &info.extent))
        return VK_ERROR_INITIALIZATION_FAILED;
    info.frameSlots = 2;
    info.model = &model;
    return ffxVkXeSSRecoveredCreate(&info, context, reason, reason_bytes);
}
```

The engine can free the file data immediately after `create_scaler` returns.
Bind `FfxVkXeSSRecoveredImages` using `ffxVkXeSSRecoveredBind`, record with
`ffxVkXeSSRecoveredRecord`, and retire GPU uses before
`ffxVkXeSSRecoveredDestroy`. C functions retain the same image/slot/synchronization
contract described above. The codec checks CRCs and table bounds; creation
checks model/header sizes and creates all GPU pipelines. Shader semantics are
trusted caller data. See [MODEL_FORMAT.md](MODEL_FORMAT.md) for the container.
