# EasyVulkan Chapter 7 Progress

## Ch7-1 Vertex Buffer
- Status: Complete
- Files changed: `CMakePresets.json`, `VKBase.h`, `GLfwGeneral.hpp`, `MyVulkan.h`, `main.cpp`, `shader/FirstTriangle.vert.shader`, `shader/FirstTriangle.frag.shader`, `PROGRESS.md`
- Build: GLSL compiled with Vulkan SDK `glslc`; both SPIR-V files passed `spirv-val`; `cmake --build --preset vs18-x64-debug` succeeded.
- Runtime: Device-local vertex buffer rendered a red/green/blue interpolated triangle; resize/maximize succeeded; continuous five-second run and graceful exit succeeded.
- Validation: `VK_LAYER_KHRONOS_validation` loaded; no ERROR or VUID was emitted during the normal run and graceful shutdown.
- Notes: Vertex data uploads through a host-visible coherent staging buffer. Buffer is destroyed before its memory. Full Device/Surface/Instance shutdown was added. Minimize kept the process alive without errors, but the automation interface could not reacquire the minimized window; minimize/restore remains a final integrated-test item.

## Ch7-2 Index Buffer
- Status: Complete
- Files changed: `MyVulkan.h`, `main.cpp`, `PROGRESS.md`
- Build: GLSL recompiled with `glslc`; SPIR-V validation passed; Debug x64 CMake build succeeded.
- Runtime: Four unique vertices and six `uint16_t` indices rendered one interpolated rectangle through `vkCmdDrawIndexed`.
- Validation: Khronos Validation loaded; no ERROR or VUID was emitted during run or graceful shutdown.
- Notes: Vertex and index data both use staging uploads into separate device-local buffers. CPU index type, bind type, and draw count are consistent.

## Ch7-3 Instancing
- Status: Complete
- Files changed: `main.cpp`, `shader/FirstTriangle.vert.shader`, `PROGRESS.md`
- Build: Both GLSL stages compiled and passed `spirv-val`; Debug x64 CMake build succeeded.
- Runtime: One indexed draw call rendered four rectangles at distinct offsets with red, green, blue, and yellow instance colors.
- Validation: Khronos Validation loaded; no ERROR or VUID was emitted during run or graceful shutdown.
- Notes: Binding 0 advances per vertex. Binding 1 advances per instance and supplies `InstanceData { offset, color }`. `instanceCount` is four; there is no CPU draw loop.

## Ch7-4 Push Constant
- Status: Not started
- Files changed: None
- Build: Not run
- Runtime: Not run
- Validation: Not run
- Notes: None

## Ch7-5 Uniform Buffer
- Status: Not started
- Files changed: None
- Build: Not run
- Runtime: Not run
- Validation: Not run
- Notes: None

## Ch7-6 Image Copy
- Status: Not started
- Files changed: None
- Build: Not run
- Runtime: Not run
- Validation: Not run
- Notes: None

## Ch7-7 Texture
- Status: Not started
- Files changed: None
- Build: Not run
- Runtime: Not run
- Validation: Not run
- Notes: None
