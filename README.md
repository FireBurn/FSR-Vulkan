# Portable FidelityFX Vulkan module

This directory is the API and portability boundary for native Vulkan FSR
work.  It is deliberately buildable without Q2RTX so that the resulting
implementation can be reused by other Vulkan applications.

Current status: the public C contract, validation layer, device capability
probe, pinned AMD 1.1.4 host scheduler, native Vulkan FSR 3 Upscaler, and
analytical Optical Flow/Frame Interpolation compute backends are implemented
and tested. The versioned source-v07 FSR4 INT8/DOT4 provider is also included.
Opaque public C APIs create, destroy, and record upscaling plus frame-generation
work without exposing an AMD type. The generated Vulkan shader tables are
checked in with manifests. Q2RTX supplies a validation-tested two-acquire,
generated-then-real reference presenter; extracting that WSI policy as a
general-purpose standalone policy library is complete, while a full generic
acquire/submit callback API remains future work.

## Design boundary

The application owns the Vulkan instance, device, queues, command buffers, and
external images.  The module owns FidelityFX contexts and their internal
resources.  Dispatch calls record work into an application-provided command
buffer.  Every imported image carries its current resource state; the runtime
must restore imported resources to that state before returning.

The API keeps these independently selectable pieces behind one contract:

1. FSR 3 temporal upscaling.
2. FSR 3 analytical frame interpolation, including optical flow.
3. A Q2RTX reference presenter that schedules real and generated frames and
   composes UI after interpolation; its reusable WSI policy library is
   available, while a complete callback API is pending.
4. The source-v07 FSR4 provider using the same Vulkan resource and temporal
   input types.

The presenter will use an explicit API rather than impersonating a Vulkan
swapchain handle.  That makes queue ownership and synchronization visible and
avoids the Windows-only behavior in AMD's old Vulkan swapchain reference.

## Build and test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
./build/ffx_vk_capability_probe
```

The standalone build creates `ffx-vulkan::portable`,
`ffx-vulkan::fsr3-host-1.1.4`, and the compute-only
`ffx-vulkan::fsr3-vk-backend-1.1.4` target. The backend exposes AMD's normal
Vulkan resource/pipeline/dispatch callbacks but deliberately leaves
`fpSwapChainConfigureFrameGeneration` null because the upstream implementation
of that callback is Win32-only. Set
`FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND=OFF` to omit the backend and generated
shader tables. In that configuration the public upscaler create/record calls
remain linkable and return `FFX_VK_PORTABLE_ERROR_UNSUPPORTED`. Set
`FFX_VK_PORTABLE_BUILD_FSR3_HOST=OFF` as well to build only the public contract
and capability probe.

SDK 2.3 analytical frame generation is separately available as
`ffx-vulkan::fsr3-vk-framegeneration-3.1.6`. Its public declarations live in
`ffx_vk_fsr3_3_1_5_bridge.h` so the opaque SDK 3.1.5 upscaler and privately
prefixed 3.1.6 FI/OF source can coexist in one application. Its lifecycle is
`create -> record prepare -> record dispatch -> retire frame -> destroy`.
`RetireFrame(completedFrameId)` is required only after the application's GPU
fence signals; monotonic frame IDs permit multiple queue-ordered frames while
it releases their temporary image views safely. RGBA8 and RGBA16F colour
images are both supported and validation-smoked. This target
records compute work but intentionally does not own a swapchain or pacing
policy. `RecordDispatch` also accepts an optional sampled `R16G16_SFLOAT`
distortion field containing `UV_after - UV_before` for lens/post-process
distortion; leave it null for the provider's neutral internal field.

The project also creates `ffx-vulkan::fsr4-v07-assets`, a dependency-free C host helper
for the source-v07 INT8 asset contract. Given a fixed model or DRS preset and
output extent, it returns the matching 1080/2160/4320 shader paths, initializer,
pass-0 weights, RCAS, and SPD names. It contains no model bytes, filesystem
policy, Vulkan dispatch backend, or claim of FSR 4.1.1 support; applications use
it to avoid accidentally mixing a graph with another model's weights.

`ffx-vulkan::fsr4-v07-vulkan` is the matching source-v07 Vulkan compute
provider. It consumes application-owned Vulkan objects and model/SPIR-V bytes,
and exposes versioned `ffxFsr4V07…` functions so it does not collide with an
AMD SDK/DLL's unversioned FFX exports. `FfxFsr4V07CreateContext` copies the
explicit `FfxInterface` installed by `ffxFsr4V07SetBackendInterface`; no
renderer global is required. It is the same target Q2RTX links.

`ffx-vulkan::framegeneration-presenter-policy` is the reusable WSI policy
layer used by Q2RTX. It does not acquire or present images for the application;
instead it codifies FIFO selection for generated→real pairs, `minImageCount +
2` requirements, pair validation, and image/GPU semaphore ownership. This
keeps platform windowing and queue submission in the host while avoiding the
common binary-semaphore and Mailbox/Immediate mistakes.

## Use in another Vulkan project

For all FSR3 paths, either vendor this directory (including `upstream/` and
`generated/`) and use `add_subdirectory`:

```cmake
add_subdirectory(extern/ffx-vulkan)
target_link_libraries(my_renderer PRIVATE
    ffx-vulkan::portable
    ffx-vulkan::fsr3-vk-framegeneration-3.1.6
    ffx-vulkan::framegeneration-presenter-policy
    ffx-vulkan::fsr4-v07-vulkan)
```

For a host that wants the complete tested FSR3+FSR4 feature closure, link the
convenience `ffx-vulkan::effects` target instead. It is demonstrated by the
independent [full-stack contract](examples/full-stack):

```sh
cmake -S extern/ffx-vulkan/examples/full-stack -B build/ffx-full-stack
cmake --build build/ffx-full-stack
./build/ffx-full-stack/ffx_vk_full_stack_contract
```

or install the complete static-library package and link the same target from a
clean C++ consumer:

```sh
cmake -S extern/ffx-vulkan -B build/ffx-vulkan
cmake --build build/ffx-vulkan
cmake --install build/ffx-vulkan --prefix /opt/ffx-vulkan
cmake -S extern/ffx-vulkan/examples/installed-full-stack -B build/consumer \
  -DCMAKE_PREFIX_PATH=/opt/ffx-vulkan
cmake --build build/consumer
./build/consumer/ffx_vk_installed_full_stack_contract
```

The complete package contains the compiled, private SDK closures and exports
only the versioned public targets and `include/` headers; a consumer needs
Vulkan but does not need Q2RTX, `upstream/`, `generated/`, or source-tree
include paths. The smaller `examples/consumer` remains an FSR4-v07 + WSI-policy
contract for C hosts that do not need the FSR3 closure.

The application retains instance/device/queue/swapchain ownership. It records
FSR3 or FSR4 work in its own command buffer, keeps imported images alive until
its frame fence signals, and composes UI after interpolation. The FSR3 public
headers document the exact resource/layout and fence-retirement contracts.

The FSR4-v07 provider also has an explicit three-frame Vulkan lifetime:
call `ffxFsr4VkBeginFrame(&interface, frame_id)` before each provider dispatch,
then call `ffxFsr4VkRetireFrame(&interface, completed_frame_id)` only after the
host fence proves those command buffers have completed. This retires descriptor
sets, staged model uploads, and the corresponding host-visible constant-buffer
partition without assuming a particular swapchain or frames-in-flight policy.

Every FSR4 external image must also be registered with
`ffxFsr4VkSetExternalImageState` before `ffxFsr4V07Dispatch`. The FFX ABI
itself contains only a `VkImageView`, so this separate record supplies the
underlying `VkImage`, current layout/stage/access, and requested restored
layout/stage/access. The provider transitions the image to `GENERAL` for its
compute passes and restores the requested state while unregistering imports.
Inputs must have `FFX_API_RESOURCE_STATE_COMPUTE_READ`; the output must have
`FFX_API_RESOURCE_STATE_UNORDERED_ACCESS`. This is an explicit host ownership
boundary, not an assumption that all renderers use Q2RTX's layout convention.

Before pipeline creation, the v07 provider reflects each supplied SPIR-V module
and rejects descriptor set/binding/type declarations outside its documented
v07 ABI. `ffxFsr4VkValidateShaderLayout` exposes the same fail-closed check to
asset loaders. It then builds a compact, binding-sorted descriptor-set layout
for that exact module rather than allocating a permissive generic layout, and
refuses a dispatch unless every declared non-sampler descriptor is populated.
This is an ABI guard for the known v07 graph, not a claim that arbitrary future
FSR4 shaders can use the provider unchanged.

The dependency-complete FSR4-v07 subset can also be installed as a CMake
package:

```sh
cmake -S extern/ffx-vulkan -B build/ffx-vulkan
cmake --build build/ffx-vulkan --target ffx_vulkan_fsr4_v07_vulkan
cmake --install build/ffx-vulkan --prefix /opt/ffx-vulkan
cmake -S extern/ffx-vulkan/examples/consumer -B build/consumer \
  -DCMAKE_PREFIX_PATH=/opt/ffx-vulkan
cmake --build build/consumer
```

The `examples/consumer` project is intentionally a compile/link contract, not
a renderer. It is a minimal starting point for a host that already owns Vulkan
initialization. The v07 provider does **not** include or install model weights
or shader binaries: obtain them under their applicable terms, keep a complete
same-preset bundle together, and load the names returned by
`ffxFsr4V07BuildAssetSet`. It is experimental source-v07 code, not FSR 4.1.1,
Ray Regeneration, or ML Frame Generation.

The tests verify the complete patched-source and generated-file hash manifests,
all 2,816 upscaler, 352 Frame Interpolation, and 56 Optical Flow valid
pass/permutation lookups, and every unique SPIR-V module with `spirv-val` when
it is installed. Separate headless tests exercise AMD's low-level backend and
the public C API. They prefer a discrete compute device, enable supported
Vulkan 1.1-1.3 features, record and submit real upscaler and two-frame
Optical-Flow/Frame-Interpolation dispatches, and require finite, nonzero,
fully overwritten RGBA16F readbacks. Both warnings and errors from
`VK_LAYER_KHRONOS_validation` fail the test.
A machine without a Vulkan 1.3 compute device reports those tests as skipped,
as does one without the required subgroup operations, storage-image formats,
or unformatted storage-image writes. The smoke procedure loader intentionally
hides the KHR-suffixed memory-requirements2 command, exercising the Vulkan 1.1
core fallback used by applications that do not enable the promoted extension.
It also enables the AMD coherent-memory extension, when present, without
enabling its optional `deviceCoherentMemory` feature. Every backend allocation
is audited and the test fails if it selects a memory type carrying
`VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD` in that configuration. Subgroup
size control and synchronization2 are physically queried but deliberately
disabled at logical-device creation. The tests require the backend to suppress
the corresponding capabilities and required-subgroup-size pipeline metadata,
and verify that disabled debug-utils and AMD buffer-marker procedures are not
even requested.

## Public upscaler and frame-generation API

`ffx_vk_portable.h` is a C11-compatible boundary. A minimal lifecycle is:

```c
FfxVkPortableDeviceInfo device_info = {
    .structSize = sizeof(device_info),
    .instance = instance,
    .physicalDevice = physical_device,
    .device = device,
    .getDeviceProcAddr = vkGetDeviceProcAddr,
    .queue = compute_queue,
    .queueFamilyIndex = compute_queue_family,
    .deviceCoherentMemoryEnabled = VK_FALSE,
    .shaderFloat16Enabled = shader_float16_was_enabled,
    .subgroupSizeControlEnabled = subgroup_size_control_was_enabled,
    .computeFullSubgroupsEnabled = compute_full_subgroups_was_enabled,
    .synchronization2Enabled = synchronization2_was_enabled,
    .bufferMarkerEnabled = amd_buffer_marker_extension_was_enabled,
    .debugUtilsEnabled = debug_utils_instance_extension_was_enabled,
    .shaderStorageBufferArrayNonUniformIndexingEnabled =
        storage_buffer_nonuniform_indexing_was_enabled,
    .accelerationStructureEnabled = acceleration_structure_was_enabled,
    .shaderStorageImageWriteWithoutFormatEnabled =
        storage_image_write_without_format_was_enabled,
};
FfxVkPortableUpscaleCreateInfo create_info = {
    .structSize = sizeof(create_info),
    .flags = FFX_VK_PORTABLE_CONTEXT_HDR_COLOR_INPUT,
    .maxRenderSize = {1920, 1080},
    .maxOutputSize = {3840, 2160},
};
FfxVkPortableUpscaleContext *upscaler = NULL;
FfxVkPortableResult result = ffxVkPortableUpscaleContextCreate(
    &device_info, &create_info, &upscaler);

/* Begin an application command buffer and fill all dispatch fields/images. */
result = ffxVkPortableUpscaleContextRecordDispatch(upscaler, &dispatch_info);

/* Submit in record order. Wait for all referencing frames before destruction. */
result = ffxVkPortableUpscaleContextDestroy(upscaler);
```

Leave `deviceCoherentMemoryEnabled` as `VK_FALSE` unless the logical device was
created with
`VkPhysicalDeviceCoherentMemoryFeaturesAMD::deviceCoherentMemory = VK_TRUE`.
Physical extension and memory-type availability do not prove that the feature
was enabled. The portable layer excludes AMD device-coherent memory types by
default because Vulkan forbids allocating them otherwise.

The remaining `*Enabled` fields have the same rule: describe the features and
extensions actually used to create the application's instance/device, not
everything returned by physical-device queries. Leave optional fields zero
when uncertain. The backend then selects conservative shader permutations and
omits optional debug/Breadcrumbs commands. In particular,
`subgroupSizeControlEnabled` controls required-subgroup-size pipeline metadata;
`computeFullSubgroupsEnabled` records the related feature for future effects
but the current upscaler does not request full-subgroup pipeline semantics.
The public FSR3 upscaler additionally requires
`shaderStorageImageWriteWithoutFormatEnabled = VK_TRUE`; its checked
accumulate modules declare that Vulkan feature, so physical-device support
without logical-device enablement is rejected at context creation.

Creation and destruction do not submit work. The context owns the backend,
scratch storage, temporal images, and FSR3's three shared upscaler outputs. It
does not own the instance, physical/logical device, queue, command buffers, or
images supplied by the application. A non-null `allocationCallbacks` pointer
must remain valid until context destruction. It is used for the portable
wrapper's shared images; AMD's pinned backend keeps its upstream behavior and
uses Vulkan's default allocator for backend-internal objects.

Each `FfxVkPortableImage.state` describes its actual layout/access state when
the record call begins. The backend records transitions back to that state
before returning. Queue-family ownership is not transferred: imported images
must already be owned by the family of the supplied command buffer.
`UNDEFINED` is intended only for first-use/discardable image contents. Calls on
one context mutate temporal/backend state and must be CPU serialized; their
command buffers must be submitted in the same order. A successful record also
requires every external image to remain alive until the recorded GPU work
completes. The API does not wait for the GPU on destruction, so the application
must do that with its normal frame fences.

The pinned backend retains dynamic external-image views for eight effect calls
rather than the upstream four. This is intentional: analytical frame
interpolation records a preparation workload and a generation workload for one
application frame. A host with two frames in flight therefore needs more than
four calls of retention before safely recycling an external view. This is a
generic Vulkan lifetime correction, not a Q2RTX-only policy.

## Reproducing the shader headers

The checked-in headers under
`generated/ffx-1.1.4/vk/fsr3upscaler/` are build inputs, not handwritten
source. Regenerate them into an empty staging directory with AMD's unmodified
v1.1.4 tools:

```sh
FFX_VK_FFX_SC=/path/to/v1.1.4/sdk/tools/binary_store/FidelityFX_SC.exe \
FFX_VK_GLSLANG_EXE=/path/to/v1.1.4/sdk/tools/binary_store/glslangValidator.exe \
./tools/generate_fsr3upscaler_shaders.sh /tmp/fsr3upscaler-generated
```

The script verifies both tool hashes, forces `-num-threads=1`, and checks all
200 output headers against `SHA256SUMS`. Wine is required on non-Windows hosts;
override its path with `FFX_VK_WINE`. The generated `.d` files are retained in
the staging directory for inspection but are not vendored because they contain
machine-specific absolute Wine paths.

The capability booleans are prerequisites, not claims of image quality or
official hardware support.  In particular, `fsr4Int8Prerequisites` only means
that the Vulkan integer-dot-product feature set needed by an INT8 path is
present.

## Resource and temporal conventions

- Motion vectors point from the current pixel to the corresponding previous
  frame pixel.  Multiplying the stored value by `motionVectorScale` produces a
  displacement in render pixels.
- Jitter is the exact sub-pixel offset used to render the current frame, in
  render-pixel units.
- Depth is the original device-depth signal.  Inverted and infinite depth are
  creation flags, not guesses made by the module.
- `frameId` increments exactly once per rendered frame.  It does not increment
  for an interpolated frame.
- `reset` is required after camera cuts, seeks, map loads, resolution changes,
  or any discontinuity in those inputs.
- FSR input color is linear scene color.  UI is supplied separately and
  composed after interpolation.

See [UPSTREAM.md](UPSTREAM.md) for the source/version import plan and license
provenance.
