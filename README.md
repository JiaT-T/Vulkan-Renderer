# Vulkan-Renderer

Minimal Vulkan Tutorial starter project configured for Visual Studio 18 Insiders, Vulkan SDK, CMake, and vcpkg.

## Local layout

- Vulkan SDK: `D:\VulkanSDK\1.4.350.0`
- vcpkg: `D:\Computer Graphics\Vulkan\.deps\vcpkg`
- vcpkg binary cache: `D:\Computer Graphics\Vulkan\.deps\vcpkg-binary-cache`

Open this folder in Visual Studio Insiders and select the `vs18-x64-debug` CMake preset. After CMake configure, Visual Studio 18 also emits `out\build\vs18-x64-debug\Vulkan-Renderer.slnx`.

The user-level `VULKAN_SDK`, `VK_SDK_PATH`, and `VK_LAYER_PATH` environment variables are configured for the D: drive. If an elevated/system environment view still shows the old `C:\VulkanSDK\1.4.350.0` value, remove or update the machine-level variables from an administrator environment.
