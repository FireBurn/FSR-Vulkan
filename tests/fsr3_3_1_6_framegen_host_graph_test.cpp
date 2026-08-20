/*
 * Copyright (c) 2026 Q2RTX FSR Vulkan contributors
 * SPDX-License-Identifier: MIT
 *
 * Exercises the public SDK 2.3 FI/OF context creation path with the actual
 * fixed-profile Vulkan blob accessors.  This is deliberately a host-graph
 * test: no Vulkan work is recorded until the separate scheduler bridge owns
 * resource allocation, descriptors, barriers, and command-buffer lifetime.
 */

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_set>

#include "ffx_frameinterpolation.h"
#include "ffx_opticalflow.h"

namespace {

constexpr uint32_t kSpirvMagic = 0x07230203u;

struct MockBackend {
    uint32_t contextsCreated = 0;
    uint32_t contextsDestroyed = 0;
    uint32_t resourcesCreated = 0;
    uint32_t resourcesDestroyed = 0;
    uint32_t pipelinesCreated = 0;
    uint32_t pipelinesDestroyed = 0;
    uint32_t spirvPipelinesCreated = 0;
    int32_t nextResource = 1;
    std::unordered_set<int32_t> liveResources;
};

MockBackend& mock(FfxInterface* backend)
{
    return *static_cast<MockBackend*>(backend->device);
}

FfxVersionNumber getSdkVersion(FfxInterface*)
{
    return FFX_SDK_MAKE_VERSION(2, 3, 0);
}

FfxErrorCode createContext(FfxInterface* backend, FfxEffect,
                           FfxEffectBindlessConfig*, FfxUInt32* outId)
{
    if (!outId)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_POINTER);
    MockBackend& state = mock(backend);
    state.contextsCreated++;
    *outId = state.contextsCreated;
    return FFX_OK;
}

FfxErrorCode destroyContext(FfxInterface* backend, FfxUInt32)
{
    mock(backend).contextsDestroyed++;
    return FFX_OK;
}

FfxErrorCode getCapabilities(FfxInterface*, FfxDeviceCapabilities* outCaps)
{
    if (!outCaps)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_POINTER);
    std::memset(outCaps, 0, sizeof(*outCaps));
    outCaps->maximumSupportedShaderModel = FFX_SHADER_MODEL_6_2;
    outCaps->waveLaneCountMin = 32;
    outCaps->waveLaneCountMax = 32;
    return FFX_OK;
}

FfxErrorCode createResource(FfxInterface* backend,
                            const FfxCreateResourceDescription*, FfxUInt32,
                            FfxResourceInternal* outResource)
{
    if (!outResource)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_POINTER);
    MockBackend& state = mock(backend);
    outResource->internalIndex = state.nextResource++;
    state.liveResources.insert(outResource->internalIndex);
    state.resourcesCreated++;
    return FFX_OK;
}

FfxErrorCode destroyResource(FfxInterface* backend, FfxResourceInternal resource,
                             FfxUInt32)
{
    MockBackend& state = mock(backend);
    if (state.liveResources.erase(resource.internalIndex) != 0)
        state.resourcesDestroyed++;
    return FFX_OK;
}

FfxErrorCode registerResource(FfxInterface*, const FfxApiResource*, FfxUInt32,
                              FfxResourceInternal* outResource)
{
    if (!outResource)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_POINTER);
    outResource->internalIndex = 1000;
    return FFX_OK;
}

FfxErrorCode unregisterResources(FfxInterface*, FfxCommandList, FfxUInt32)
{
    return FFX_OK;
}

FfxApiResourceDescription getResourceDescription(FfxInterface*, FfxResourceInternal)
{
    return {};
}

FfxErrorCode stageConstantBuffer(FfxInterface*, void*, FfxUInt32 size,
                                 FfxConstantBuffer* outBuffer)
{
    if (!size || !outBuffer)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_ARGUMENT);
    outBuffer->num32BitEntries = size / sizeof(uint32_t);
    return FFX_OK;
}

FfxErrorCode createPipeline(FfxInterface* backend, FfxShaderBlob* blob,
                            const FfxPipelineDescription*, FfxUInt32,
                            FfxPipelineState* outPipeline)
{
    if (!blob || !blob->data || blob->size < sizeof(uint32_t) || !outPipeline)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_POINTER);
    uint32_t magic = 0;
    std::memcpy(&magic, blob->data, sizeof(magic));
    if (magic != kSpirvMagic)
        return static_cast<FfxErrorCode>(FFX_ERROR_INVALID_ARGUMENT);
    MockBackend& state = mock(backend);
    state.pipelinesCreated++;
    state.spirvPipelinesCreated++;
    std::memset(outPipeline, 0, sizeof(*outPipeline));
    outPipeline->pipeline = reinterpret_cast<FfxPipeline>(
        static_cast<uintptr_t>(state.pipelinesCreated));
    return FFX_OK;
}

FfxErrorCode destroyPipeline(FfxInterface* backend, FfxPipelineState* pipeline,
                             FfxUInt32)
{
    if (pipeline && pipeline->pipeline)
        mock(backend).pipelinesDestroyed++;
    return FFX_OK;
}

FfxErrorCode scheduleGpuJob(FfxInterface*, const FfxGpuJobDescription*)
{
    return FFX_OK;
}

FfxErrorCode executeGpuJobs(FfxInterface*, FfxCommandList, FfxUInt32)
{
    return FFX_OK;
}

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FSR3.1.6 FI/OF host graph failure: %s\n", message);
        std::abort();
    }
}

} // namespace

FfxErrorCode GetResourceSizeFromDescription(FfxDevice,
                                            const FfxCreateResourceDescription*,
                                            uint64_t* sizeInBytes,
                                            uint64_t* alignment)
{
    if (sizeInBytes)
        *sizeInBytes = 0;
    if (alignment)
        *alignment = 0;
    return FFX_OK;
}

int main()
{
    MockBackend state{};
    FfxInterface backend{};
    backend.device = &state;
    backend.fpGetSDKVersion = getSdkVersion;
    backend.fpCreateBackendContext = createContext;
    backend.fpDestroyBackendContext = destroyContext;
    backend.fpGetDeviceCapabilities = getCapabilities;
    backend.fpCreateResource = createResource;
    backend.fpDestroyResource = destroyResource;
    backend.fpRegisterResource = registerResource;
    backend.fpUnregisterResources = unregisterResources;
    backend.fpGetResourceDescription = getResourceDescription;
    backend.fpStageConstantBufferDataFunc = stageConstantBuffer;
    backend.fpCreatePipeline = createPipeline;
    backend.fpDestroyPipeline = destroyPipeline;
    backend.fpScheduleGpuJob = scheduleGpuJob;
    backend.fpExecuteGpuJobs = executeGpuJobs;

    FfxOpticalflowContextDescription ofDesc{};
    ofDesc.backendInterface = backend;
    ofDesc.resolution = {1280, 720};
    FfxOpticalflowContext opticalFlow{};
    expect(ffxOpticalflowContextCreate(&opticalFlow, &ofDesc) == FFX_OK,
           "optical-flow context creation");

    FfxFrameInterpolationContextDescription fiDesc{};
    fiDesc.backendInterface = backend;
    fiDesc.maxRenderSize = {640, 360};
    fiDesc.displaySize = {1280, 720};
    fiDesc.backBufferFormat = FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    fiDesc.previousInterpolationSourceFormat = FFX_API_SURFACE_FORMAT_R8G8B8A8_UNORM;
    FfxFrameInterpolationContext frameInterpolation{};
    expect(ffxFrameInterpolationContextCreate(&frameInterpolation, &fiDesc) == FFX_OK,
           "frame-interpolation context creation");

    expect(state.contextsCreated == 2, "one backend context per effect");
    expect(state.pipelinesCreated == 18, "all 11 FI and 7 OF pipelines");
    expect(state.spirvPipelinesCreated == 18, "every pipeline used an embedded SPIR-V blob");
    expect(state.resourcesCreated > 0, "persistent FI/OF resources created");

    expect(ffxFrameInterpolationContextDestroy(&frameInterpolation) == FFX_OK,
           "frame-interpolation context destruction");
    expect(ffxOpticalflowContextDestroy(&opticalFlow) == FFX_OK,
           "optical-flow context destruction");
    expect(state.resourcesDestroyed == state.resourcesCreated,
           "all persistent resources released");
    expect(state.pipelinesDestroyed == state.pipelinesCreated,
           "all non-null pipelines released");
    expect(state.contextsDestroyed == state.contextsCreated,
           "all backend contexts released");

    std::printf("FSR3.1.6 FI/OF graph: %u resources, %u embedded-SPIR-V pipelines\n",
                state.resourcesCreated, state.pipelinesCreated);
    return 0;
}
