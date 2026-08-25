# Installed full-stack FSR Vulkan contract

This verifies the complete FSR3 + source-v07 FSR4 CMake package from a clean
consumer build. It does not use the vendored source tree after installation.

```sh
cmake -S extern/ffx-vulkan -B build/ffx-vulkan
cmake --build build/ffx-vulkan
cmake --install build/ffx-vulkan --prefix /opt/ffx-vulkan
cmake -S extern/ffx-vulkan/examples/installed-full-stack -B build/consumer \
  -DCMAKE_PREFIX_PATH=/opt/ffx-vulkan
cmake --build build/consumer
./build/consumer/ffx_vk_installed_full_stack_contract
```

Link `ffx-vulkan::effects` in a C++ Vulkan application for the complete tested
FSR3.1.4, FSR3.1.5, FSR3.1.6 FI/OF, source-v07 FSR4, and frame-generation WSI
policy closure. The application remains responsible for Vulkan device/queue,
external-image state, fences, and presentation integration.
