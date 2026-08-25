cmake_minimum_required(VERSION 3.15)

project(ffx_vulkan_portable VERSION 0.1.0 LANGUAGES C CXX)

option(FFX_VK_PORTABLE_BUILD_TESTS "Build the portable FidelityFX Vulkan contract tests" ON)
option(FFX_VK_PORTABLE_BUILD_PROBE "Build the Vulkan device capability probe" ON)
option(FFX_VK_PORTABLE_BUILD_FSR3_HOST "Build the pinned AMD FSR 3.1.4 host scheduler" ON)
option(FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND "Build the pinned AMD FSR 3.1.4 native Vulkan upscaler backend" ON)
option(FFX_VK_PORTABLE_BUILD_FSR3_3_1_5_SCAFFOLD
    "Compile the public SDK 2.3 FSR 3.1.5 host source as a standalone port scaffold" ON)
option(FFX_VK_PORTABLE_BUILD_FSR4_V07_VULKAN
    "Build the experimental source-v07 FSR4 INT8/DOT4 Vulkan provider" ON)
option(FFX_VK_PORTABLE_INSTALL
    "Install portable headers, libraries, and a CMake package" ON)

find_package(Vulkan REQUIRED)

# This fixed public-SDK profile is an embedded portable-backend asset, not a
# test-only development file. The generator updates this directory atomically.
set(FFX_2_3_0_GENERATED_FSR3UPSCALER
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/ffx-2.3.0/vk/fsr3upscaler-q2-v2")
set(FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION
    "${CMAKE_CURRENT_SOURCE_DIR}/generated/ffx-2.3.0/vk/fsr3framegeneration-q2-v1")

# The source-bearing v07 FSR4 model itself is intentionally not vendored into
# this reusable module. This target supplies the small, provenance-neutral host
# contract that selects one coherent generated shader/weight bundle.
add_library(ffx_vulkan_fsr4_v07_assets STATIC
    src/ffx_vk_fsr4_v07_schedule.c
    src/ffx_vk_fsr4_v07_assets.c
)
add_library(ffx-vulkan::fsr4-v07-assets ALIAS ffx_vulkan_fsr4_v07_assets)
set_property(TARGET ffx_vulkan_fsr4_v07_assets PROPERTY
    EXPORT_NAME fsr4-v07-assets)
target_compile_features(ffx_vulkan_fsr4_v07_assets PUBLIC c_std_11)
target_include_directories(ffx_vulkan_fsr4_v07_assets PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(MSVC)
    target_compile_options(ffx_vulkan_fsr4_v07_assets PRIVATE /W4)
else()
    target_compile_options(ffx_vulkan_fsr4_v07_assets PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The policy is intentionally smaller than a swapchain wrapper: it lets an
# application retain ownership of platform WSI while sharing the invariants
# required for a generated-then-real presentation pair.
add_library(ffx_vulkan_framegeneration_presenter_policy STATIC
    src/ffx_vk_framegeneration_presenter.c)
add_library(ffx-vulkan::framegeneration-presenter-policy ALIAS
    ffx_vulkan_framegeneration_presenter_policy)
set_property(TARGET ffx_vulkan_framegeneration_presenter_policy PROPERTY
    EXPORT_NAME framegeneration-presenter-policy)
target_compile_features(ffx_vulkan_framegeneration_presenter_policy PUBLIC c_std_11)
target_include_directories(ffx_vulkan_framegeneration_presenter_policy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(ffx_vulkan_framegeneration_presenter_policy PUBLIC Vulkan::Vulkan)
if(MSVC)
    target_compile_options(ffx_vulkan_framegeneration_presenter_policy PRIVATE /W4)
else()
    target_compile_options(ffx_vulkan_framegeneration_presenter_policy PRIVATE
        -Wall -Wextra -Wpedantic)
endif()

if(FFX_VK_PORTABLE_BUILD_FSR4_V07_VULKAN)
    add_library(ffx_vulkan_fsr4_v07_vulkan STATIC
        src/ffx_vk_fsr4_v07.c
        src/ffx_vk_fsr4_v07_provider.c
    )
    add_library(ffx-vulkan::fsr4-v07-vulkan ALIAS
        ffx_vulkan_fsr4_v07_vulkan)
    set_property(TARGET ffx_vulkan_fsr4_v07_vulkan PROPERTY
        EXPORT_NAME fsr4-v07-vulkan)
    target_compile_features(ffx_vulkan_fsr4_v07_vulkan PUBLIC c_std_11)
    target_include_directories(ffx_vulkan_fsr4_v07_vulkan
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(ffx_vulkan_fsr4_v07_vulkan
        PUBLIC ffx-vulkan::fsr4-v07-assets Vulkan::Vulkan)
    if(UNIX AND NOT APPLE)
        target_link_libraries(ffx_vulkan_fsr4_v07_vulkan PUBLIC m)
    endif()
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr4_v07_vulkan PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr4_v07_vulkan PRIVATE
            -Wall -Wextra -Wpedantic)
    endif()
endif()

add_library(ffx_vulkan_portable STATIC
    src/ffx_vk_portable.c
    src/ffx_vk_fsr3_3_1_5_reflection.c
    src/ffx_vk_fsr3_3_1_5_catalog.c
    src/ffx_vk_fsr3_3_1_5_pipeline.c
    src/ffx_vk_fsr3_3_1_5_bundle.c
    src/ffx_vk_fsr3_3_1_5_descriptor.c
)
add_library(ffx-vulkan::portable ALIAS ffx_vulkan_portable)

target_compile_features(ffx_vulkan_portable PUBLIC c_std_11)
target_include_directories(ffx_vulkan_portable
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${FFX_2_3_0_GENERATED_FSR3UPSCALER}"
)
target_link_libraries(ffx_vulkan_portable PUBLIC Vulkan::Vulkan)

if(MSVC)
    target_compile_options(ffx_vulkan_portable PRIVATE /W4)
else()
    target_compile_options(ffx_vulkan_portable PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(FFX_VK_PORTABLE_INSTALL)
    include(CMakePackageConfigHelpers)
    include(GNUInstallDirs)

    set(FFX_VK_PORTABLE_INSTALL_TARGETS
        ffx_vulkan_fsr4_v07_assets
        ffx_vulkan_framegeneration_presenter_policy)
    if(FFX_VK_PORTABLE_BUILD_FSR4_V07_VULKAN)
        list(APPEND FFX_VK_PORTABLE_INSTALL_TARGETS
            ffx_vulkan_fsr4_v07_vulkan)
    endif()

    install(TARGETS ${FFX_VK_PORTABLE_INSTALL_TARGETS}
        EXPORT ffx-vulkan-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT ffx-vulkan-targets
        FILE ffx-vulkan-targets.cmake
        NAMESPACE ffx-vulkan::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffx-vulkan)
    configure_package_config_file(
        cmake/ffx-vulkan-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ffx-vulkan-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffx-vulkan)
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/ffx-vulkan-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/ffx-vulkan-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/ffx-vulkan-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ffx-vulkan)
endif()

if(FFX_VK_PORTABLE_BUILD_FSR3_HOST)
    set(FFX_1_1_4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-1.1.4/sdk")
    add_library(ffx_vulkan_fsr3_host_1_1_4 STATIC
        "${FFX_1_1_4_ROOT}/src/shared/ffx_assert.cpp"
        "${FFX_1_1_4_ROOT}/src/shared/ffx_message.cpp"
        "${FFX_1_1_4_ROOT}/src/shared/ffx_object_management.cpp"
        "${FFX_1_1_4_ROOT}/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp"
        "${FFX_1_1_4_ROOT}/src/components/opticalflow/ffx_opticalflow.cpp"
        "${FFX_1_1_4_ROOT}/src/components/frameinterpolation/ffx_frameinterpolation.cpp"
        "${FFX_1_1_4_ROOT}/src/components/fsr3/ffx_fsr3.cpp"
    )
    add_library(ffx-vulkan::fsr3-host-1.1.4 ALIAS ffx_vulkan_fsr3_host_1_1_4)
    set_target_properties(ffx_vulkan_fsr3_host_1_1_4 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_include_directories(ffx_vulkan_fsr3_host_1_1_4
        PUBLIC "${FFX_1_1_4_ROOT}/include"
        PRIVATE
            "${FFX_1_1_4_ROOT}/src/shared"
            "${FFX_1_1_4_ROOT}/src/components"
    )
    if(NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ffx_vulkan_fsr3_host_1_1_4 PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
        )
    endif()
endif()

if(FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND)
    if(NOT FFX_VK_PORTABLE_BUILD_FSR3_HOST)
        message(FATAL_ERROR "FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND requires FFX_VK_PORTABLE_BUILD_FSR3_HOST")
    endif()

    set(FFX_1_1_4_GENERATED_FSR3UPSCALER
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/ffx-1.1.4/vk/fsr3upscaler")
    set(FFX_1_1_4_GENERATED_FSR3FRAMEGENERATION
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/ffx-1.1.4/vk/fsr3framegeneration")
    add_library(ffx_vulkan_fsr3_vk_backend_1_1_4 STATIC
        "${FFX_1_1_4_ROOT}/src/backends/vk/ffx_vk.cpp"
        "${FFX_1_1_4_ROOT}/src/backends/shared/ffx_shader_blobs.cpp"
        "${FFX_1_1_4_ROOT}/src/backends/shared/blob_accessors/ffx_fsr3upscaler_shaderblobs.cpp"
        "${FFX_1_1_4_ROOT}/src/backends/shared/blob_accessors/ffx_frameinterpolation_shaderblobs.cpp"
        "${FFX_1_1_4_ROOT}/src/backends/shared/blob_accessors/ffx_opticalflow_shaderblobs.cpp"
        "${FFX_1_1_4_ROOT}/src/shared/ffx_breadcrumbs_list.cpp"
    )
    add_library(ffx-vulkan::fsr3-vk-backend-1.1.4 ALIAS ffx_vulkan_fsr3_vk_backend_1_1_4)
    set_target_properties(ffx_vulkan_fsr3_vk_backend_1_1_4 PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(ffx_vulkan_fsr3_vk_backend_1_1_4 PRIVATE
        FFX_FSR3UPSCALER=1
        FFX_FI=1
        FFX_OF=1
    )
    target_include_directories(ffx_vulkan_fsr3_vk_backend_1_1_4
        PUBLIC "${FFX_1_1_4_ROOT}/include"
        PRIVATE
            "${FFX_1_1_4_ROOT}/src/shared"
            "${FFX_1_1_4_ROOT}/src/components"
            "${FFX_1_1_4_ROOT}/src/backends/shared"
            "${FFX_1_1_4_ROOT}/src/backends/shared/blob_accessors"
            "${FFX_1_1_4_GENERATED_FSR3UPSCALER}"
            "${FFX_1_1_4_GENERATED_FSR3FRAMEGENERATION}"
    )
    target_link_libraries(ffx_vulkan_fsr3_vk_backend_1_1_4
        PUBLIC ffx-vulkan::fsr3-host-1.1.4 Vulkan::Vulkan
    )
    if(NOT WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(ffx_vulkan_fsr3_vk_backend_1_1_4 PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
        )
    endif()

    target_sources(ffx_vulkan_portable PRIVATE
        src/ffx_vk_fsr3_portable.cpp
    )
    set_target_properties(ffx_vulkan_portable PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(ffx_vulkan_portable PRIVATE
        FFX_VK_PORTABLE_HAS_FSR3_UPSCALER=1
    )
    target_link_libraries(ffx_vulkan_portable PRIVATE
        ffx-vulkan::fsr3-vk-backend-1.1.4
    )
endif()

# SDK 2.3's upscaler is deliberately compiled as an object-only scaffold at
# this stage.  Its public FfxInterface ABI differs from the 1.1.4 Vulkan
# backend, and its generated shader tables are DX12-oriented.  Object
# compilation catches source/API drift without pretending that it is already
# safe to link against the proven 1.1.4 Vulkan path.
if(FFX_VK_PORTABLE_BUILD_FSR3_3_1_5_SCAFFOLD)
    set(FFX_2_3_0_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-2.3.0/Kits/FidelityFX")
    # SDK 1.1.4 and SDK 2.3 export the same unversioned C ABI symbols
    # (ffxFsr3UpscalerContextCreate et al.).  Q2RTX deliberately links the
    # proven 1.1.4 implementation beside this newer public-SDK experiment, so
    # make the complete 3.1.5 closure private at link time.  This is not an
    # API rename for consumers: ffx_vk_fsr3_3_1_5_bridge.h remains the stable,
    # opaque, versioned interface.
    set(FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS
        ffxAssertReport=ffxVk315AssertReport
        ffxAssertSetPrintingCallback=ffxVk315AssertSetPrintingCallback
        ffxFsr3UpscalerContextCreate=ffxVk315Fsr3UpscalerContextCreate
        ffxFsr3UpscalerContextDestroy=ffxVk315Fsr3UpscalerContextDestroy
        ffxFsr3UpscalerContextDispatch=ffxVk315Fsr3UpscalerContextDispatch
        ffxFsr3UpscalerContextGenerateReactiveMask=ffxVk315Fsr3UpscalerContextGenerateReactiveMask
        ffxFsr3UpscalerContextGetGpuMemoryUsage=ffxVk315Fsr3UpscalerContextGetGpuMemoryUsage
        ffxFsr3UpscalerGetGpuMemoryUsage=ffxVk315Fsr3UpscalerGetGpuMemoryUsage
        ffxFsr3UpscalerGetJitterOffset=ffxVk315Fsr3UpscalerGetJitterOffset
        ffxFsr3UpscalerGetJitterPhaseCount=ffxVk315Fsr3UpscalerGetJitterPhaseCount
        ffxFsr3UpscalerGetRenderResolutionFromQualityMode=ffxVk315Fsr3UpscalerGetRenderResolutionFromQualityMode
        ffxFsr3UpscalerGetSharedResourceDescriptions=ffxVk315Fsr3UpscalerGetSharedResourceDescriptions
        ffxFsr3UpscalerGetUpscaleRatioFromQualityMode=ffxVk315Fsr3UpscalerGetUpscaleRatioFromQualityMode
        ffxFsr3UpscalerResourceIsNull=ffxVk315Fsr3UpscalerResourceIsNull
        ffxFsr3UpscalerSetConstant=ffxVk315Fsr3UpscalerSetConstant
        ffxFsr3UpscalerSetGlobalDebugMessage=ffxVk315Fsr3UpscalerSetGlobalDebugMessage
        frameInterpolationGetPermutationBlobByIndex=ffxVk316FrameInterpolationGetPermutationBlobByIndex
        frameInterpolationIsWave64=ffxVk316FrameInterpolationIsWave64
        opticalflowGetPermutationBlobByIndex=ffxVk316OpticalflowGetPermutationBlobByIndex
        opticalflowIsWave64=ffxVk316OpticalflowIsWave64
        ffxFrameInterpolationContextCreate=ffxVk316FrameInterpolationContextCreate
        ffxFrameInterpolationContextDestroy=ffxVk316FrameInterpolationContextDestroy
        ffxFrameInterpolationContextEnqueueRefreshPipelineRequest=ffxVk316FrameInterpolationContextEnqueueRefreshPipelineRequest
        ffxFrameInterpolationContextGetGpuMemoryUsage=ffxVk316FrameInterpolationContextGetGpuMemoryUsage
        ffxFrameInterpolationDispatch=ffxVk316FrameInterpolationDispatch
        ffxFrameInterpolationGetGpuMemoryUsage=ffxVk316FrameInterpolationGetGpuMemoryUsage
        ffxFrameInterpolationGetSharedResourceDescriptions=ffxVk316FrameInterpolationGetSharedResourceDescriptions
        ffxFrameInterpolationPrepare=ffxVk316FrameInterpolationPrepare
        ffxFrameInterpolationResourceIsNull=ffxVk316FrameInterpolationResourceIsNull
        ffxFrameInterpolationSetGlobalDebugMessage=ffxVk316FrameInterpolationSetGlobalDebugMessage
        ffxOpticalflowContextCreate=ffxVk316OpticalflowContextCreate
        ffxOpticalflowContextDestroy=ffxVk316OpticalflowContextDestroy
        ffxOpticalflowContextDispatch=ffxVk316OpticalflowContextDispatch
        ffxOpticalflowContextGetGpuMemoryUsage=ffxVk316OpticalflowContextGetGpuMemoryUsage
        ffxOpticalflowGetGpuMemoryUsage=ffxVk316OpticalflowGetGpuMemoryUsage
        ffxOpticalflowGetSharedResourceDescriptions=ffxVk316OpticalflowGetSharedResourceDescriptions
        ffxOpticalflowResourceIsNull=ffxVk316OpticalflowResourceIsNull
        ffxSharedContextGetGpuMemoryUsage=ffxVk316SharedContextGetGpuMemoryUsage
        ffxPrintMessage=ffxVk315PrintMessage
        ffxSafeReleaseCopyResource=ffxVk315SafeReleaseCopyResource
        ffxSafeReleasePipeline=ffxVk315SafeReleasePipeline
        ffxSafeReleaseResource=ffxVk315SafeReleaseResource
        ffxSetPrintMessageCallback=ffxVk315SetPrintMessageCallback
    )
    add_library(ffx_vulkan_fsr3_host_3_1_5_scaffold OBJECT
        "${FFX_2_3_0_ROOT}/api/internal/ffx_assert.cpp"
        "${FFX_2_3_0_ROOT}/api/internal/ffx_message.cpp"
        "${FFX_2_3_0_ROOT}/api/internal/ffx_object_management.cpp"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal/ffx_fsr3upscaler.cpp"
    )
    add_library(ffx-vulkan::fsr3-host-3.1.5-scaffold ALIAS
        ffx_vulkan_fsr3_host_3_1_5_scaffold)
    set_target_properties(ffx_vulkan_fsr3_host_3_1_5_scaffold PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(ffx_vulkan_fsr3_host_3_1_5_scaffold PRIVATE
        FFX_VK_PORTABLE_NO_WATERMARK=1
        ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS}
    )
    target_include_directories(ffx_vulkan_fsr3_host_3_1_5_scaffold PRIVATE
        "${FFX_2_3_0_ROOT}/api/include"
        "${FFX_2_3_0_ROOT}/api/internal"
        "${FFX_2_3_0_ROOT}/upscalers/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
    )
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr3_host_3_1_5_scaffold PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr3_host_3_1_5_scaffold PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
            -Wall -Wextra -Wpedantic)
    endif()

    # FI/OF use the same public SDK core helpers as the upscaler but must not
    # pull the upscaler scheduler itself into their host-graph executable: it
    # would require its independent shader-blob callback.  Keep a small core
    # object set for the FI/OF scheduler gate.  It deliberately has the same
    # private helper namespace and is only ever linked into a separate test or
    # a future 3.1.6 backend target.
    add_library(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold OBJECT
        "${FFX_2_3_0_ROOT}/api/internal/ffx_assert.cpp"
        "${FFX_2_3_0_ROOT}/api/internal/ffx_message.cpp"
        "${FFX_2_3_0_ROOT}/api/internal/ffx_object_management.cpp"
    )
    set_target_properties(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold PRIVATE
        FFX_VK_PORTABLE_NO_WATERMARK=1
        ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS}
    )
    target_include_directories(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold PRIVATE
        "${FFX_2_3_0_ROOT}/api/include"
        "${FFX_2_3_0_ROOT}/api/internal"
        "${FFX_2_3_0_ROOT}/upscalers/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
    )
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
            -Wall -Wextra -Wpedantic)
    endif()

    # Keep the newer FI/OF source port separate from the linked upscaler
    # scaffold.  Both source files deliberately refer to their own generated
    # shader-blob accessors, which must not be faked merely to make the
    # upscaler graph test link.  This object-only target is the first strict
    # portability gate while the genuine Vulkan shader catalogue is built.
    add_library(ffx_vulkan_fsr3_framegen_3_1_6_scaffold OBJECT
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal/ffx_frameinterpolation.cpp"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal/ffx_opticalflow.cpp"
        src/ffx_vk_fsr3_3_1_6_framegen_shaderblobs.cpp
    )
    add_library(ffx-vulkan::fsr3-framegen-3.1.6-scaffold ALIAS
        ffx_vulkan_fsr3_framegen_3_1_6_scaffold)
    set_target_properties(ffx_vulkan_fsr3_framegen_3_1_6_scaffold PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_compile_definitions(ffx_vulkan_fsr3_framegen_3_1_6_scaffold PRIVATE
        FFX_VK_PORTABLE_NO_WATERMARK=1
        ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS}
    )
    target_include_directories(ffx_vulkan_fsr3_framegen_3_1_6_scaffold PRIVATE
        "${FFX_2_3_0_ROOT}/api/include"
        "${FFX_2_3_0_ROOT}/api/internal"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
        "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
        "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
    )
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr3_framegen_3_1_6_scaffold PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr3_framegen_3_1_6_scaffold PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
            -Wall -Wextra -Wpedantic)
    endif()

    add_library(ffx_vulkan_fsr3_vk_backend_3_1_5 STATIC
        src/ffx_vk_fsr3_3_1_5_bridge.cpp
        src/ffx_vk_fsr3_3_1_5_upscaler.cpp
        $<TARGET_OBJECTS:ffx_vulkan_fsr3_host_3_1_5_scaffold>
    )
    add_library(ffx-vulkan::fsr3-vk-backend-3.1.5 ALIAS
        ffx_vulkan_fsr3_vk_backend_3_1_5)
    set_target_properties(ffx_vulkan_fsr3_vk_backend_3_1_5 PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
    )
    target_include_directories(ffx_vulkan_fsr3_vk_backend_3_1_5 PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
        "${FFX_2_3_0_ROOT}/api/include"
        "${FFX_2_3_0_ROOT}/api/internal"
        "${FFX_2_3_0_ROOT}/upscalers/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
        "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
    )
    target_include_directories(ffx_vulkan_fsr3_vk_backend_3_1_5 PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${FFX_2_3_0_ROOT}/api/include>
    )
    target_link_libraries(ffx_vulkan_fsr3_vk_backend_3_1_5
        PUBLIC ffx-vulkan::portable Vulkan::Vulkan
    )
    target_compile_definitions(ffx_vulkan_fsr3_vk_backend_3_1_5 PRIVATE
        ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr3_vk_backend_3_1_5 PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr3_vk_backend_3_1_5 PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
            -Wall -Wextra -Wpedantic)
    endif()

    # A separate reusable 3.1.6 FI/OF library.  It owns the SDK 2.3 core
    # closure because the 3.1.5 upscaler scaffold carries the same private
    # helpers and cannot be linked beside it without duplicate definitions.
    add_library(ffx_vulkan_fsr3_vk_framegeneration_3_1_6 STATIC
        src/ffx_vk_fsr3_3_1_5_bridge.cpp
        src/ffx_vk_fsr3_3_1_6_framegen.cpp
        $<TARGET_OBJECTS:ffx_vulkan_fsr3_framegen_3_1_6_scaffold>
        $<TARGET_OBJECTS:ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold>
    )
    add_library(ffx-vulkan::fsr3-vk-framegeneration-3.1.6 ALIAS
        ffx_vulkan_fsr3_vk_framegeneration_3_1_6)
    set_target_properties(ffx_vulkan_fsr3_vk_framegeneration_3_1_6 PROPERTIES
        CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
    target_include_directories(ffx_vulkan_fsr3_vk_framegeneration_3_1_6
        PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
            "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}")
    target_link_libraries(ffx_vulkan_fsr3_vk_framegeneration_3_1_6
        PUBLIC ffx-vulkan::portable Vulkan::Vulkan)
    target_compile_definitions(ffx_vulkan_fsr3_vk_framegeneration_3_1_6 PRIVATE
        ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
    if(MSVC)
        target_compile_options(ffx_vulkan_fsr3_vk_framegeneration_3_1_6 PRIVATE /W4)
    else()
        target_compile_options(ffx_vulkan_fsr3_vk_framegeneration_3_1_6 PRIVATE
            -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
            -Wall -Wextra -Wpedantic)
    endif()
endif()

if(FFX_VK_PORTABLE_BUILD_TESTS)
    enable_testing()

    add_test(NAME ffx_vk_fsr3_vendored_hashes
        COMMAND "${CMAKE_COMMAND}"
            "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-1.1.4/CURRENT_SHA256SUMS"
            "-DBASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-1.1.4"
            -DEXPECTED_COUNT=122
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
    )

    if(FFX_VK_PORTABLE_BUILD_FSR3_3_1_5_SCAFFOLD)
        add_test(NAME ffx_vk_fsr3_3_1_5_vendored_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-2.3.0/CURRENT_SHA256SUMS"
                "-DBASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/upstream/ffx-2.3.0"
                -DEXPECTED_COUNT=166
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )

        add_executable(ffx_vk_fsr3_3_1_5_header_test tests/fsr3_3_1_5_header_test.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_5_header_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_include_directories(ffx_vk_fsr3_3_1_5_header_test PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/upscalers/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
        )
        add_test(NAME ffx_vk_fsr3_3_1_5_header_test
            COMMAND ffx_vk_fsr3_3_1_5_header_test)

        add_executable(ffx_vk_fsr3_3_1_5_host_graph_test
            tests/fsr3_3_1_5_host_graph_test.cpp
            $<TARGET_OBJECTS:ffx_vulkan_fsr3_host_3_1_5_scaffold>)
        set_target_properties(ffx_vk_fsr3_3_1_5_host_graph_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(ffx_vk_fsr3_3_1_5_host_graph_test PRIVATE
            FFX_VK_PORTABLE_NO_WATERMARK=1
            ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
        target_include_directories(ffx_vk_fsr3_3_1_5_host_graph_test PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/upscalers/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
        )
        add_test(NAME ffx_vk_fsr3_3_1_5_host_graph_test
            COMMAND ffx_vk_fsr3_3_1_5_host_graph_test)

        # Exercise the real fixed-profile blob accessors used by the imported
        # 3.1.6 FI/OF scheduler.  Keep this separately linked from the host
        # scaffold: a scheduler/backend bridge is still a later milestone.
        add_executable(ffx_vk_fsr3_3_1_6_framegen_blob_test
            tests/fsr3_3_1_6_framegen_blob_test.cpp
            src/ffx_vk_fsr3_3_1_6_framegen_shaderblobs.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_blob_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(ffx_vk_fsr3_3_1_6_framegen_blob_test PRIVATE
            FFX_VK_PORTABLE_NO_WATERMARK=1
            ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
        target_include_directories(ffx_vk_fsr3_3_1_6_framegen_blob_test PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
            "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
        )
        if(MSVC)
            target_compile_options(ffx_vk_fsr3_3_1_6_framegen_blob_test PRIVATE /W4)
        else()
            target_compile_options(ffx_vk_fsr3_3_1_6_framegen_blob_test PRIVATE
                -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
                -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_blob_test
            COMMAND ffx_vk_fsr3_3_1_6_framegen_blob_test)

        add_executable(ffx_vk_fsr3_3_1_6_framegen_host_graph_test
            tests/fsr3_3_1_6_framegen_host_graph_test.cpp
            $<TARGET_OBJECTS:ffx_vulkan_fsr3_framegen_3_1_6_scaffold>
            $<TARGET_OBJECTS:ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold>)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_host_graph_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(ffx_vk_fsr3_3_1_6_framegen_host_graph_test PRIVATE
            FFX_VK_PORTABLE_NO_WATERMARK=1
            ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
        target_include_directories(ffx_vk_fsr3_3_1_6_framegen_host_graph_test PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/upscalers/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
            "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
        )
        if(MSVC)
            target_compile_options(ffx_vk_fsr3_3_1_6_framegen_host_graph_test PRIVATE /W4)
        else()
            target_compile_options(ffx_vk_fsr3_3_1_6_framegen_host_graph_test PRIVATE
                -include "${CMAKE_CURRENT_SOURCE_DIR}/src/ffx_1_1_4_portability.hpp"
                -Wall -Wextra -Wpedantic)
        endif()
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_host_graph_test
            COMMAND ffx_vk_fsr3_3_1_6_framegen_host_graph_test)

        add_executable(ffx_vk_fsr3_3_1_6_framegen_pipeline_test
            tests/fsr3_3_1_6_framegen_pipeline_test.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_pipeline_test PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_include_directories(ffx_vk_fsr3_3_1_6_framegen_pipeline_test PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}")
        target_link_libraries(ffx_vk_fsr3_3_1_6_framegen_pipeline_test PRIVATE
            ffx-vulkan::portable Vulkan::Vulkan)
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_pipeline_test
            COMMAND ffx_vk_fsr3_3_1_6_framegen_pipeline_test)
        set_tests_properties(ffx_vk_fsr3_3_1_6_framegen_pipeline_test PROPERTIES
            RUN_SERIAL TRUE SKIP_RETURN_CODE 77)

        add_test(NAME ffx_vk_fsr3_3_1_5_generated_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3UPSCALER}/SHA256SUMS"
                "-DBASE_DIR=${FFX_2_3_0_GENERATED_FSR3UPSCALER}"
                -DEXPECTED_COUNT=11
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_source_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}/SOURCE_SHA256SUMS"
                "-DBASE_DIR=${FFX_2_3_0_ROOT}"
                -DEXPECTED_COUNT=18
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_generated_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}/SHA256SUMS"
                "-DBASE_DIR=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
                -DEXPECTED_COUNT=18
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_embedded_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}/EMBEDDED_SOURCE_SHA256SUMS"
                "-DBASE_DIR=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
                -DEXPECTED_COUNT=18
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )
        find_program(FFX_VK_3_1_5_SPIRV_VAL spirv-val)
        if(FFX_VK_3_1_5_SPIRV_VAL)
        add_test(NAME ffx_vk_fsr3_3_1_5_spirv_val
                COMMAND "${CMAKE_COMMAND}"
                    "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3UPSCALER}/SHA256SUMS"
                    "-DBASE_DIR=${FFX_2_3_0_GENERATED_FSR3UPSCALER}"
                    "-DVALIDATOR=${FFX_VK_3_1_5_SPIRV_VAL}"
                    -DEXPECTED_COUNT=11
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_spirv_manifest.cmake"
            )
            add_test(NAME ffx_vk_fsr3_3_1_6_framegen_spirv_val
                COMMAND "${CMAKE_COMMAND}"
                    "-DMANIFEST=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}/SHA256SUMS"
                    "-DBASE_DIR=${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}"
                    "-DVALIDATOR=${FFX_VK_3_1_5_SPIRV_VAL}"
                    -DEXPECTED_COUNT=18
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_spirv_manifest.cmake"
            )
        endif()

        add_executable(ffx_vk_fsr3_3_1_5_reflection_test
            tests/fsr3_3_1_5_reflection_test.c)
        target_compile_definitions(ffx_vk_fsr3_3_1_5_reflection_test PRIVATE
            FSR3_315_SPV_DIR="${FFX_2_3_0_GENERATED_FSR3UPSCALER}"
            FSR3_316_FRAMEGEN_SPV_DIR="${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}")
        target_link_libraries(ffx_vk_fsr3_3_1_5_reflection_test PRIVATE
            ffx-vulkan::portable)
        add_test(NAME ffx_vk_fsr3_3_1_5_reflection_test
            COMMAND ffx_vk_fsr3_3_1_5_reflection_test)
        add_executable(ffx_vk_fsr3_3_1_5_catalog_test tests/fsr3_3_1_5_catalog_test.c)
        target_link_libraries(ffx_vk_fsr3_3_1_5_catalog_test PRIVATE ffx-vulkan::portable)
        add_test(NAME ffx_vk_fsr3_3_1_5_catalog_test COMMAND ffx_vk_fsr3_3_1_5_catalog_test)

        add_executable(ffx_vk_fsr3_3_1_5_bundle_test tests/fsr3_3_1_5_bundle_test.c)
        target_compile_definitions(ffx_vk_fsr3_3_1_5_bundle_test PRIVATE
            FSR3_315_SPV_DIR="${FFX_2_3_0_GENERATED_FSR3UPSCALER}")
        target_link_libraries(ffx_vk_fsr3_3_1_5_bundle_test PRIVATE ffx-vulkan::portable)
        add_test(NAME ffx_vk_fsr3_3_1_5_bundle_test COMMAND ffx_vk_fsr3_3_1_5_bundle_test)

        add_executable(ffx_vk_fsr3_3_1_5_pipeline_test tests/fsr3_3_1_5_pipeline_test.c)
        target_compile_definitions(ffx_vk_fsr3_3_1_5_pipeline_test PRIVATE
            FSR3_315_SPV_DIR="${FFX_2_3_0_GENERATED_FSR3UPSCALER}")
        target_link_libraries(ffx_vk_fsr3_3_1_5_pipeline_test PRIVATE ffx-vulkan::portable)
        add_test(NAME ffx_vk_fsr3_3_1_5_pipeline_test COMMAND ffx_vk_fsr3_3_1_5_pipeline_test)
        set_tests_properties(ffx_vk_fsr3_3_1_5_pipeline_test PROPERTIES
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77)
        add_executable(ffx_vk_fsr3_3_1_5_bridge_test
            tests/fsr3_3_1_5_bridge_test.cpp
            $<TARGET_OBJECTS:ffx_vulkan_fsr3_framegen_3_1_6_scaffold>
            $<TARGET_OBJECTS:ffx_vulkan_fsr3_sdk_core_3_1_5_scaffold>)
        set_target_properties(ffx_vk_fsr3_3_1_5_bridge_test PROPERTIES
            CXX_STANDARD 20
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO)
        target_include_directories(ffx_vk_fsr3_3_1_5_bridge_test PRIVATE
            "${FFX_2_3_0_ROOT}/api/include"
            "${FFX_2_3_0_ROOT}/api/internal"
            "${FFX_2_3_0_ROOT}/upscalers/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/include"
            "${FFX_2_3_0_ROOT}/upscalers/fsr3/internal"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/include"
            "${FFX_2_3_0_ROOT}/framegeneration/fsr3/internal"
            "${FFX_2_3_0_GENERATED_FSR3FRAMEGENERATION}")
        target_compile_definitions(ffx_vk_fsr3_3_1_5_bridge_test PRIVATE
            ${FFX_3_1_5_PRIVATE_SYMBOL_DEFINITIONS})
        target_link_libraries(ffx_vk_fsr3_3_1_5_bridge_test PRIVATE
            ffx-vulkan::fsr3-vk-backend-3.1.5)
        add_test(NAME ffx_vk_fsr3_3_1_5_bridge_test COMMAND ffx_vk_fsr3_3_1_5_bridge_test)
        set_tests_properties(ffx_vk_fsr3_3_1_5_bridge_test PROPERTIES
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77)

        add_executable(ffx_vk_fsr3_3_1_6_framegen_api_smoke
            tests/fsr3_3_1_6_framegen_api_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_api_smoke PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_link_libraries(ffx_vk_fsr3_3_1_6_framegen_api_smoke PRIVATE
            ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_api_smoke
            COMMAND ffx_vk_fsr3_3_1_6_framegen_api_smoke)
        set_tests_properties(ffx_vk_fsr3_3_1_6_framegen_api_smoke PROPERTIES
            RUN_SERIAL TRUE SKIP_RETURN_CODE 77)

        add_executable(ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke
            tests/fsr3_3_1_6_framegen_api_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_definitions(ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke PRIVATE
            FSR316_SMOKE_FLOAT16=1)
        target_link_libraries(ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke PRIVATE
            ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke
            COMMAND ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke)
        set_tests_properties(ffx_vk_fsr3_3_1_6_framegen_rgba16f_api_smoke PROPERTIES
            RUN_SERIAL TRUE SKIP_RETURN_CODE 77)

        # Q2RTX keeps normalized motion in FLAT_MOTION.RG and depth derivatives
        # in BA.  Exercise that exact sampled-input format as well as its
        # RGBA16F HDRless scene/output presentation image.
        add_executable(ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke
            tests/fsr3_3_1_6_framegen_api_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_definitions(ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke PRIVATE
            FSR316_SMOKE_FLOAT16=1
            FSR316_SMOKE_MOTION_RGBA16F=1)
        target_link_libraries(ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke PRIVATE
            ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke
            COMMAND ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke)
        set_tests_properties(ffx_vk_fsr3_3_1_6_framegen_q2rtx_motion_api_smoke PROPERTIES
            RUN_SERIAL TRUE SKIP_RETURN_CODE 77)

        add_executable(ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke
            tests/fsr3_3_1_6_framegen_api_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke PROPERTIES
            CXX_STANDARD 20 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO)
        target_compile_definitions(ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke PRIVATE
            FSR316_SMOKE_FLOAT16=1
            FSR316_SMOKE_MOTION_RGBA16F=1
            FSR316_SMOKE_SCRGB=1)
        target_link_libraries(ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke PRIVATE
            ffx-vulkan::fsr3-vk-framegeneration-3.1.6)
        add_test(NAME ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke
            COMMAND ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke)
        set_tests_properties(ffx_vk_fsr3_3_1_6_framegen_scrgb_api_smoke PROPERTIES
            RUN_SERIAL TRUE SKIP_RETURN_CODE 77)
    endif()

    add_executable(ffx_vk_contract_test tests/contract_test.c)
    target_link_libraries(ffx_vk_contract_test PRIVATE ffx-vulkan::portable)
    add_test(NAME ffx_vk_contract_test COMMAND ffx_vk_contract_test)

    add_executable(ffx_vk_fsr4_v07_assets_test
        tests/fsr4_v07_assets_test.c)
    target_link_libraries(ffx_vk_fsr4_v07_assets_test PRIVATE
        ffx-vulkan::fsr4-v07-assets)
    add_test(NAME ffx_vk_fsr4_v07_assets_test COMMAND ffx_vk_fsr4_v07_assets_test)

    add_executable(ffx_vk_framegeneration_presenter_policy_test
        tests/framegeneration_presenter_policy_test.c)
    target_link_libraries(ffx_vk_framegeneration_presenter_policy_test PRIVATE
        ffx-vulkan::framegeneration-presenter-policy)
    add_test(NAME ffx_vk_framegeneration_presenter_policy_test
        COMMAND ffx_vk_framegeneration_presenter_policy_test)

    if(FFX_VK_PORTABLE_BUILD_FSR3_HOST)
        add_executable(ffx_vk_fsr3_host_test tests/fsr3_host_test.cpp)
        set_target_properties(ffx_vk_fsr3_host_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_link_libraries(ffx_vk_fsr3_host_test PRIVATE ffx-vulkan::fsr3-host-1.1.4)
        add_test(NAME ffx_vk_fsr3_host_test COMMAND ffx_vk_fsr3_host_test)
    endif()

    if(FFX_VK_PORTABLE_BUILD_FSR3_VK_BACKEND)
        add_executable(ffx_vk_fsr3_shader_blob_test tests/fsr3_shader_blob_test.cpp)
        set_target_properties(ffx_vk_fsr3_shader_blob_test PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_include_directories(ffx_vk_fsr3_shader_blob_test PRIVATE
            "${FFX_1_1_4_ROOT}/src/backends/shared"
        )
        target_link_libraries(ffx_vk_fsr3_shader_blob_test PRIVATE
            ffx-vulkan::fsr3-vk-backend-1.1.4
        )
        add_test(NAME ffx_vk_fsr3_shader_blob_test COMMAND ffx_vk_fsr3_shader_blob_test)

        add_test(NAME ffx_vk_fsr3_generated_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_1_1_4_GENERATED_FSR3UPSCALER}/SHA256SUMS"
                "-DBASE_DIR=${FFX_1_1_4_GENERATED_FSR3UPSCALER}"
                -DEXPECTED_COUNT=200
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )

        add_executable(ffx_vk_fsr3_backend_smoke tests/fsr3_vk_backend_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_backend_smoke PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_link_libraries(ffx_vk_fsr3_backend_smoke PRIVATE
            ffx-vulkan::fsr3-vk-backend-1.1.4
            ffx-vulkan::portable
        )
        add_test(NAME ffx_vk_fsr3_backend_smoke COMMAND ffx_vk_fsr3_backend_smoke)
        set_tests_properties(ffx_vk_fsr3_backend_smoke PROPERTIES
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77
        )

        add_executable(ffx_vk_fsr3_portable_api_smoke tests/fsr3_vk_backend_smoke.cpp)
        set_target_properties(ffx_vk_fsr3_portable_api_smoke PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED YES
            CXX_EXTENSIONS NO
        )
        target_compile_definitions(ffx_vk_fsr3_portable_api_smoke PRIVATE
            FFX_VK_PORTABLE_ONLY=1
        )
        target_link_libraries(ffx_vk_fsr3_portable_api_smoke PRIVATE
            ffx-vulkan::portable
        )
        add_test(NAME ffx_vk_fsr3_portable_api_smoke COMMAND ffx_vk_fsr3_portable_api_smoke)
        set_tests_properties(ffx_vk_fsr3_portable_api_smoke PROPERTIES
            RUN_SERIAL TRUE
            SKIP_RETURN_CODE 77
        )

        find_program(FFX_VK_SPIRV_VAL spirv-val)
        if(FFX_VK_SPIRV_VAL)
            add_test(NAME ffx_vk_fsr3_spirv_val
                COMMAND "${CMAKE_COMMAND}"
                    "-DDUMPER=$<TARGET_FILE:ffx_vk_fsr3_shader_blob_test>"
                    "-DVALIDATOR=${FFX_VK_SPIRV_VAL}"
                    "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/fsr3upscaler-spv"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/validate_spirv.cmake"
            )

        add_test(NAME ffx_vk_fsr3_framegen_generated_hashes
            COMMAND "${CMAKE_COMMAND}"
                "-DMANIFEST=${FFX_1_1_4_GENERATED_FSR3FRAMEGENERATION}/SHA256SUMS"
                "-DBASE_DIR=${FFX_1_1_4_GENERATED_FSR3FRAMEGENERATION}"
                -DEXPECTED_COUNT=164
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/verify_sha256.cmake"
        )
        endif()
    endif()
endif()

if(FFX_VK_PORTABLE_BUILD_PROBE)
    add_executable(ffx_vk_capability_probe tools/capability_probe.c)
    target_link_libraries(ffx_vk_capability_probe PRIVATE ffx-vulkan::portable)
endif()
