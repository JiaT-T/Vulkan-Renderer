# EasyVulkan Chapter 6 and 7 Progress

## Ch6-0 Modern Feature Queries
- Status: Complete
- Files changed: `VKStart.h`, `helper.h`, `VKBase.h`, `MyVulkan.h`, `main.cpp`, `PROGRESS.md`
- Build commands: Vulkan SDK `glslc` for both GLSL stages; `spirv-val` for both generated SPIR-V files; `cmake --build --preset vs18-x64-debug --config Debug`.
- Runtime command: `Debug/Vulkan-Renderer.exe --self-test`, launched from the generated project directory with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`.
- Runtime: Process exited with code 0. The self-test logged resize to 960x540, minimize, restore, and graceful shutdown. Desktop automation was intentionally not used; final visual inspection remains a user-run check.
- Validation: `ch6-0.validation.log` contained 0 matches for `ERROR`, `VUID`, `WARNING`, or `Validation Error`.
- Device support: Loader 1.4.350; requested API 1.3.0; NVIDIA GeForce RTX 5070 Ti Laptop GPU, device API 1.4.325, driver 591.86. `samplerAnisotropy`, `imagelessFramebuffer`, and `dynamicRendering` are supported.
- Enabled state: `samplerAnisotropy=true`; `imagelessFramebuffer=false`; `dynamicRendering=false`; enabled device extension is `VK_KHR_swapchain`.
- Notes: Queries now use `VkPhysicalDeviceFeatures2`, Vulkan 1.1/1.2/1.3 feature structures, `VkPhysicalDeviceProperties2`, and `VkPhysicalDeviceMemoryProperties2`. A reusable `pNextChain` prevents duplicate objects and, by default, duplicate `sType` values. Supported and enabled feature structures are stored separately. Existing Ch7 synchronization and shutdown validation issues found by the first logged run were corrected.

## Ch6-1 Imageless Framebuffer
- Status: Implemented; user verification pending
- Files changed: `VKBase.h`, `MyVulkan.h`, `main.cpp`, `PROGRESS.md`
- Run mode: `--mode=imageless` requests `ImagelessFramebuffer`; `--mode=legacy` keeps the traditional comparison path.
- Core/extension selection: Vulkan 1.2+ uses `VkPhysicalDeviceVulkan12Features::imagelessFramebuffer`. The Vulkan 1.1 fallback checks `VK_KHR_image_format_list` and `VK_KHR_imageless_framebuffer`, then enables `VkPhysicalDeviceImagelessFramebufferFeatures`. Unsupported requests log the reason and select `LegacyRenderPass`.
- Implementation: One framebuffer is created with `VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT`, `VkFramebufferAttachmentsCreateInfo`, and `VkFramebufferAttachmentImageInfo`. The current swapchain image view is supplied through `VkRenderPassAttachmentBeginInfo` when the render pass begins.
- Swapchain lifecycle: The single imageless framebuffer is destroyed before old swapchain image views and recreated from the new format requirements, extent, usage, and layer count. The render pass remains device-lifetime in this project because swapchain recreation retains the selected surface format.
- Build/runtime/validation: Not run after this section per the latest user instruction; to be verified by the user.

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
- Status: Complete
- Files changed: `main.cpp`, `shader/FirstTriangle.vert.shader`, `PROGRESS.md`
- Build: GLSL compilation and `spirv-val` passed; Debug x64 CMake build succeeded.
- Runtime: Two observations 1.8 seconds apart showed changing rectangle scale and color modulation while preserving one indexed instanced draw.
- Validation: Khronos Validation loaded; no ERROR or VUID was emitted during run or graceful shutdown.
- Notes: A 32-byte explicitly aligned push block is registered for the vertex stage. Its size is checked against `maxPushConstantsSize` and updated every command-buffer recording.

## Ch7-5 Uniform Buffer
- Status: Complete
- Files changed: `MyVulkan.h`, `main.cpp`, `shader/FirstTriangle.vert.shader`, `PROGRESS.md`
- Build: GLSL compilation and `spirv-val` passed; Debug x64 CMake build succeeded.
- Runtime: Model rotation visibly changed between observations while Push Constant scale/color and indexed instancing remained active.
- Validation: Khronos Validation loaded; no ERROR or VUID was emitted during run or graceful shutdown.
- Notes: The renderer has one frame in flight, so it owns one persistently mapped host-coherent UBO and one descriptor set. Binding 0 is a vertex-stage Uniform Buffer containing aligned Model/View/Projection matrices. The frame fence completes before the next UBO write. Vulkan projection Y is flipped explicitly.

## Ch7-6 Image Copy
- Status: Complete
- Files changed: `CMakeLists.txt`, `stb_image.cpp`, `MyVulkan.h`, `main.cpp`, `PROGRESS.md`
- Build: GLSL compilation and `spirv-val` passed; CMake regenerated and Debug x64 build succeeded with the stb implementation translation unit.
- Runtime: `viking_room.png` was loaded as RGBA8, copied from a host staging buffer into a device-local image, then linearly blitted from its native extent to the swapchain extent and visibly presented before the instanced scene.
- Validation: Khronos Validation loaded; no ERROR, VUID, or image-layout warning was emitted during the logged run and graceful shutdown.
- Notes: Source and destination format features are checked before linear blit. Barriers use TOP_OF_PIPE→TRANSFER for undefined destinations, TRANSFER→TRANSFER for copy-to-blit visibility, and TRANSFER→BOTTOM_OF_PIPE before present. The image view is destroyed before its image, and the image before its memory.

## Ch7-7 Texture
- Status: Complete; final visual inspection remains user-run
- Files changed: `assets/uv_orientation_test.png`, `CMakeLists.txt`, `MyVulkan.h`, `main.cpp`, `shader/FirstTriangle.vert.shader`, `shader/FirstTriangle.frag.shader`, `PROGRESS.md`
- Build: Both GLSL stages compile successfully and Debug x64 builds successfully as part of Ch6-0 validation.
- Runtime: The existing textured indexed instanced path completed the Ch6-0 self-test and exited normally; desktop visual inspection was not performed per the latest user instruction.
- Validation: Khronos Validation reported no ERROR, VUID, or WARNING in the Ch6-0 legacy-path run.
- Notes: Vertex data now includes UVs. Descriptor set binding 0 remains the UBO and binding 1 is a Combined Image Sampler. `texture2d` loads RGBA8 through stb, uploads through a staging buffer, creates a device-local sampled image, computes 11 mip levels for the 1024x1024 orientation chart, checks linear-blit format support, generates each mip with explicit per-level barriers, creates an all-mip image view, and creates a linear mipmapped sampler with feature-gated anisotropy. The final shader multiplies sampled texture color by instance and Push Constant tint. Ch7-6 boot-image blit remains in the startup path.
