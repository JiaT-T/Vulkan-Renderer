# EasyVulkan Chapter 6 and 7 Progress

## Ch8-1 Offscreen Rendering
- Status: Implemented; user verification pending
- Files changed: `CMakeLists.txt`, `Chapter8.h`, `Chapter8.cpp`, `main.cpp`, `shader/Offscreen.vert.shader`, `shader/Offscreen.frag.shader`, `shader/Fullscreen.vert.shader`, `shader/Fullscreen.frag.shader`, `PROGRESS.md`
- Shader: Added a procedural offscreen scene shader and a full-screen triangle sampling shader. CMake now discovers all `*.vert.shader` and `*.frag.shader` sources and maps them to same-named SPIR-V outputs.
- Build: Not run; validation requirements were explicitly excluded by the user.
- Runtime: Not run. Select with `--example=offscreen`.
- Validation: Not run.
- Resize: Swapchain callbacks destroy and recreate the offscreen image, memory, view, framebuffer, descriptor pool/set, screen framebuffers, render passes, and extent-dependent pipelines.
- Notes: The scene is drawn only into an `R8G8B8A8_UNORM` image with `COLOR_ATTACHMENT|SAMPLED` usage. Its render-pass final layout and dependency make color writes visible to the following fragment shader. A separate full-screen pass is the only draw that writes the sampled result to the swapchain. The renderer keeps one frame in flight, so the single offscreen target is not concurrently read and written by multiple frames.

## Ch8-2 Depth Test and Visualization
- Status: Implemented; user verification pending
- Files changed: `MyVulkan.h`, `Chapter8.cpp`, `shader/DepthScene.vert.shader`, `shader/DepthScene.frag.shader`, `shader/DepthVisualize.frag.shader`, `PROGRESS.md`
- Shader: Added instanced cube MVP/normal transformation, simple directional lighting, and raw/linearized perspective-depth display.
- Build: Not run; validation requirements were explicitly excluded by the user.
- Runtime: Not run. Select `--example=depth`, `--example=depth-raw`, or `--example=depth-linear`.
- Validation: Not run.
- Resize: Color/depth images, views, framebuffer, descriptors, screen framebuffers, render passes, and fixed-extent pipelines are recreated by swapchain callbacks.
- Notes: The renderer selects `D32_SFLOAT` or `D16_UNORM` only when both depth-attachment and sampled-image features are present. The image uses `DEPTH_STENCIL_ATTACHMENT|SAMPLED`, a depth-only aspect, clear value 1.0, `LESS`, depth test/write, back-face culling, and CCW front faces. Three independently transformed cubes share one indexed instanced draw. Raw depth and near/far-consistent linear depth are sampled in a separate full-screen pass.

## Ch8-3 Deferred Rendering
- Status: Implemented; user verification pending
- Files changed: `Chapter8.cpp`, `shader/DeferredGeometry.vert.shader`, `shader/DeferredGeometry.frag.shader`, `shader/DeferredComposition.frag.shader`, `PROGRESS.md`
- Shader: Geometry pass writes Albedo, world-space Normal, and world Position. Composition reads four input attachments, including Depth, and provides lighting/Albedo/Normal/Position debug modes.
- Build: Not run; validation requirements were explicitly excluded by the user.
- Runtime: Not run. Select `--example=deferred`, `--example=gbuffer-albedo`, `--example=gbuffer-normal`, or `--example=gbuffer-position`.
- Validation: Not run.
- Resize: All G-buffer images, depth image, views, descriptors, per-swapchain framebuffers, render pass, and pipelines are recreated.
- Notes: A traditional render pass contains Geometry subpass 0 and Composition subpass 1. Albedo uses `R8G8B8A8_UNORM`; Normal and Position use `R16G16B16A16_SFLOAT`; Depth uses the selected depth format. Four `INPUT_ATTACHMENT` descriptors and matching `input_attachment_index` values feed the composition shader. A precise by-region dependency exposes geometry color/depth writes to input-attachment reads. Lighting is limited to ambient plus one directional diffuse term; transparency, PBR, shadows, and MSAA are intentionally outside this stage.

## Ch8-4 Premultiplied Alpha
- Status: Implemented; user verification pending
- Files changed: `CMakeLists.txt`, `MyVulkan.h`, `Chapter8.cpp`, `shader/AlphaTest.vert.shader`, `shader/AlphaTest.frag.shader`, `PROGRESS.md`
- Shader: Added a three-panel procedural quad shader. The fragment shader returns the selected texture representation unchanged and explicitly avoids repeated premultiplication.
- Build: Not run; validation requirements were explicitly excluded by the user.
- Runtime: Not run. Select `--example=alpha`.
- Validation: Not run.
- Notes: The existing transparent nettle PNG is loaded twice. The straight copy remains unchanged; the premultiplied copy converts each 8-bit RGB channel with `(RGB*A+127)/255` before mip 0 is uploaded, so all later mip levels filter premultiplied data. The three panels are: straight texture + straight blend, straight texture + intentionally wrong premultiplied blend, and premultiplied texture + correct premultiplied blend. Color factors are respectively `SRC_ALPHA` or `ONE`, both with `ONE_MINUS_SRC_ALPHA`; alpha uses `ONE` and `ONE_MINUS_SRC_ALPHA`. Panels do not overlap, while the notes retain the requirement that real transparent objects still need back-to-front sorting.

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

## Ch6-2 Dynamic Rendering
- Status: Implemented; user verification pending
- Files changed: `VKBase.h`, `main.cpp`, `PROGRESS.md`, `docs/EasyVulkan_Chapter6_Summary.md`
- Run mode: Dynamic Rendering is the default. `--mode=dynamic`, `--mode=imageless`, and `--mode=legacy` explicitly select the requested path; unsupported new modes fall back to `LegacyRenderPass` with a log message.
- Core/extension selection: Vulkan 1.3+ uses `VkPhysicalDeviceVulkan13Features::dynamicRendering`. Vulkan 1.2 checks and enables `VK_KHR_dynamic_rendering` plus `VkPhysicalDeviceDynamicRenderingFeatures`. Core or KHR command entry points are loaded through `vkGetDeviceProcAddr` and checked before the mode remains active.
- Pipeline: The dynamic path sets `VkGraphicsPipelineCreateInfo::renderPass` to `VK_NULL_HANDLE` and chains `VkPipelineRenderingCreateInfo` with the swapchain color format and undefined depth/stencil formats.
- Command recording: `VkRenderingAttachmentInfo` supplies the current swapchain image view and clear/store behavior; `VkRenderingInfo` supplies render area, layer count, and attachment list; the path calls the loaded Begin/End Rendering entry points around the unchanged Ch7 indexed instanced textured draw.
- Layout/synchronization: Before rendering, only the current color subresource transitions from `UNDEFINED` to `COLOR_ATTACHMENT_OPTIMAL` with TOP_OF_PIPE→COLOR_ATTACHMENT_OUTPUT and color-write destination access. `UNDEFINED` is intentional because the attachment is cleared and old contents are discarded. After rendering it transitions to `PRESENT_SRC_KHR` with COLOR_ATTACHMENT_OUTPUT→BOTTOM_OF_PIPE and color-write source access.
- Swapchain lifecycle: No `VkRenderPass` or `VkFramebuffer` is created in this path. Swapchain image views are recreated by `graphicsBase`; the extent-dependent fixed viewport/scissor pipeline is recreated by its existing callbacks; render area is read from the current swapchain extent each frame.
- Preserved Ch7 work: Vertex/index/instance buffers, Push Constant, UBO, descriptor set, texture, sampler, mipmaps, boot-image blit, and `vkCmdDrawIndexed` remain shared by all three modes.
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
