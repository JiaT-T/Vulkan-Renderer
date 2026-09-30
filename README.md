# Vulkan-Renderer

C++20 Vulkan rendering experiments: explicit GPU resource management, offscreen passes, depth and deferred rendering, alpha blending, and SDR/HDR output. The repository follows the EasyVulkan learning sequence and keeps the comparison examples and notes alongside the implementation.

## Rendering examples

Select an example with `--example=<name>`. The default is `forward`.

| Example | What the code demonstrates |
| --- | --- |
| `forward` | Indexed instanced textured drawing, UBOs and push constants; legacy render pass, imageless framebuffer or dynamic rendering |
| `offscreen` | Render to an image, then sample it in a fullscreen pass |
| `depth`, `depth-off` | Compare enabled and disabled depth testing |
| `depth-raw`, `depth-linear` | Visualize raw and linearized depth |
| `deferred` | Geometry pass writing albedo, normal and position attachments; fullscreen lighting composition |
| `gbuffer-albedo`, `gbuffer-normal`, `gbuffer-position` | Inspect individual G-buffer attachments |
| `alpha` | Compare alpha testing, straight alpha and premultiplied alpha |
| `srgb` | Compare linear/sRGB output handling |
| `sdr` | HDR intermediate target followed by SDR tone mapping |
| `hdr` | Request HDR surface output, with SDR fallback if unavailable |

There is no checked-in renderer screenshot yet. Run the examples above to inspect the current scenes; the PNGs under `assets/` are input textures rather than claimed render results.

## Technical overview

`VKBase.h` owns device/swapchain selection, feature queries and lifecycle callbacks. `MyVulkan.h` supplies buffer, image, descriptor, shader and pipeline helpers. `main.cpp` retains the forward learning path; `RenderExample.cpp` dispatches feature examples to `RenderFeatureRenderer.cpp` and the individual renderer sections. GLSL sources under `shader/` compile to SPIR-V during the build.

The forward path accepts `--mode=legacy`, `--mode=imageless` and `--mode=dynamic` (default), and falls back when a requested feature is unavailable. The other examples currently use traditional render passes. `--hdr=auto`, `--hdr=sdr` and `--hdr=request` control output preference. These are rendering experiments, not a complete scene engine or an OBJ/glTF loader: those files in `assets/` are retained learning assets.

## Build and run

Requirements:

- CMake 3.29 or newer and a C++20 compiler with `<format>` support.
- Vulkan SDK with headers **1.4.335 or newer**, Vulkan loader, and `glslc`; `VULKAN_SDK` must identify the SDK installation. The header requirement does not mean all rendering modes require a Vulkan 1.4 GPU.
- A Vulkan-capable GPU and driver for running the windowed examples.
- A bootstrapped [vcpkg](https://github.com/microsoft/vcpkg) checkout, exposed as `VCPKG_ROOT`. The manifest installs GLFW, GLM and stb; the registry baseline pins dependency resolution.
- Visual Studio 2022 with Desktop development with C++ for the Windows presets, or Ninja and a configured compiler environment for the Ninja presets.

From the cloned repository, in PowerShell:

```powershell
# Set these to YOUR installations, or persist them in your own environment.
$env:VULKAN_SDK = '<Vulkan SDK installation>'
$env:VCPKG_ROOT = '<bootstrapped vcpkg checkout>'
cmake --preset windows-debug
cmake --build --preset windows-debug
Set-Location out/build/windows-debug/Vulkan-Renderer
./Debug/Vulkan-Renderer.exe --example=deferred
```

The multi-configuration Visual Studio generator puts the executable in `Debug` or `Release`. Runtime textures and shaders are in its parent directory; the program must run from that parent directory:

```powershell
./Debug/Vulkan-Renderer.exe --example=deferred
./Debug/Vulkan-Renderer.exe --example=forward --mode=dynamic --self-test
```

For a Release build, use `windows-release` for both configure/build, then run `./Release/Vulkan-Renderer.exe` from `out/build/windows-release/Vulkan-Renderer`. For Ninja, use `ninja-debug` or `ninja-release` and run `./Vulkan-Renderer` (or `.exe`) from `out/build/<preset>/Vulkan-Renderer`. On Windows, invoke Ninja presets from an x64 developer shell. `--self-test` applies to the forward path and automatically exercises its resize/exit sequence; it does not validate every feature example.

Public presets inherit your SDK environment and locate the toolchain through `VCPKG_ROOT`. Keep compiler-instance paths, triplet overrides or binary-cache settings in the ignored `CMakeUserPresets.json`; do not commit private machine paths. A user preset can inherit `windows-debug` or `ninja-debug` and override its generator or cache variables. To use preinstalled dependencies without vcpkg, configure directly with `cmake -S . -B out/build/local` and provide their package paths through `CMAKE_PREFIX_PATH`.

## Validation and limits

The source contains all examples listed above. `PROGRESS.md` records the original per-chapter development and validation history; some entries explicitly remain pending. In the portfolio cleanup environment, preset parsing was checked, but Vulkan configure/build/runtime could not be verified because the Vulkan SDK and vcpkg dependencies were unavailable. HDR still requires a compatible display, OS configuration, driver and surface format; request fallback is not evidence of HDR display acceptance. Resize, minimize/restore and validation-layer checks should be performed per example on a supported machine.

## References and assets

- [EasyVulkan](https://easyvulkan.github.io/) supplies the tutorial structure behind the Vulkan helpers and chapter notes. The comparison modes and changes here are recorded in [PROGRESS.md](PROGRESS.md) and [docs/](docs/).
- [Khronos Vulkan Tutorial](https://github.com/KhronosGroup/Vulkan-Tutorial) and its CMake dependency helpers are additional learning/build references.
- GLFW, GLM, stb and Vulkan SDK components retain their upstream licenses. The bundled `CMake/FindVulkan.cmake` carries a BSD license reference in its header.
- The existing [LICENSE](LICENSE) is preserved. Source URLs and individual redistribution licenses for the Viking room, plant/table and UV test assets are not recorded in this repository. Their rights need confirmation; the repository license does not establish permission for third-party assets. No new third-party assets are copied into this cleanup.
