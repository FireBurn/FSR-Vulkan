# Verify the exported package from a copied, self-contained consumer example.
# The generated directory is rooted under the known CMake build directory and
# contains only test artefacts, so it is safe to recreate on every CTest run.
foreach(_ffx_vk_required FFX_VK_PACKAGE_SOURCE_DIR FFX_VK_PACKAGE_BUILD_DIR)
    if(NOT DEFINED ${_ffx_vk_required} OR "${${_ffx_vk_required}}" STREQUAL "")
        message(FATAL_ERROR "${_ffx_vk_required} is required")
    endif()
endforeach()

set(_ffx_vk_test_root
    "${FFX_VK_PACKAGE_BUILD_DIR}/installed-full-stack-consumer-test")
set(_ffx_vk_prefix "${_ffx_vk_test_root}/prefix")
set(_ffx_vk_source "${_ffx_vk_test_root}/source")
set(_ffx_vk_build "${_ffx_vk_test_root}/build")

file(REMOVE_RECURSE "${_ffx_vk_test_root}")
file(MAKE_DIRECTORY "${_ffx_vk_test_root}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${FFX_VK_PACKAGE_BUILD_DIR}"
            --prefix "${_ffx_vk_prefix}"
    RESULT_VARIABLE _ffx_vk_install_result
    OUTPUT_VARIABLE _ffx_vk_install_output
    ERROR_VARIABLE _ffx_vk_install_error)
if(NOT _ffx_vk_install_result EQUAL 0)
    message(FATAL_ERROR "ffx-vulkan installation failed:\n${_ffx_vk_install_output}${_ffx_vk_install_error}")
endif()

file(COPY "${FFX_VK_PACKAGE_SOURCE_DIR}/examples/installed-full-stack/"
    DESTINATION "${_ffx_vk_source}")
execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${_ffx_vk_source}" -B "${_ffx_vk_build}"
            "-DCMAKE_PREFIX_PATH=${_ffx_vk_prefix}"
    RESULT_VARIABLE _ffx_vk_configure_result
    OUTPUT_VARIABLE _ffx_vk_configure_output
    ERROR_VARIABLE _ffx_vk_configure_error)
if(NOT _ffx_vk_configure_result EQUAL 0)
    message(FATAL_ERROR "installed consumer configure failed:\n${_ffx_vk_configure_output}${_ffx_vk_configure_error}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_ffx_vk_build}"
    RESULT_VARIABLE _ffx_vk_build_result
    OUTPUT_VARIABLE _ffx_vk_build_output
    ERROR_VARIABLE _ffx_vk_build_error)
if(NOT _ffx_vk_build_result EQUAL 0)
    message(FATAL_ERROR "installed consumer build failed:\n${_ffx_vk_build_output}${_ffx_vk_build_error}")
endif()

execute_process(
    COMMAND "${_ffx_vk_build}/ffx_vk_installed_full_stack_contract"
    RESULT_VARIABLE _ffx_vk_run_result
    OUTPUT_VARIABLE _ffx_vk_run_output
    ERROR_VARIABLE _ffx_vk_run_error)
if(NOT _ffx_vk_run_result EQUAL 0)
    message(FATAL_ERROR "installed consumer failed:\n${_ffx_vk_run_output}${_ffx_vk_run_error}")
endif()
