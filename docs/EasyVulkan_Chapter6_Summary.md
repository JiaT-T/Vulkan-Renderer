# EasyVulkan 第六章学习总结

本文对应当前 `Vulkan-Renderer` 工程的真实实现，而不是教程代码的孤立复述。工程在开始第六章前已经具备第七章的带纹理、索引、实例化绘制路径：顶点/索引/实例缓冲区、Push Constant、Uniform Buffer、Descriptor Set、Image/Sampler/Mipmap 和启动图像 Blit 均已存在。本次修改在保留这些功能的前提下，新增现代能力查询和三种可切换的屏幕渲染模式。

## 1. 第六章解决的问题

第六章把工程从“只会用固定的传统 Render Pass 路径”推进到三个层面：

1. 正确查询 Loader、物理设备和不同 Vulkan 版本的能力，并只开启实际需要且受支持的设备特性。
2. 用 Imageless Framebuffer 减少交换链图像对应的 Framebuffer 对象数量。
3. 用 Dynamic Rendering 取消对传统 `VkRenderPass` 和 `VkFramebuffer` 对象的依赖，并显式负责附件描述、图像布局转换和同步。

最终运行时模式为：

| 模式 | 参数 | Render Pass 对象 | Framebuffer 对象 | 默认选择 |
|---|---|---:|---:|---:|
| `LegacyRenderPass` | `--mode=legacy` | 1 | 每张交换链图像 1 个 | 否，兼容回退 |
| `ImagelessFramebuffer` | `--mode=imageless` | 1 | 整个交换链 1 个 | 否，Ch6-1 对照 |
| `DynamicRendering` | `--mode=dynamic` | 0 | 0 | 是 |

若请求的新模式不受设备支持，`graphicsBase::ConfigureRequestedRenderMode()` 会输出原因并把活动模式改为 `LegacyRenderPass`，而不是强行开启特性或崩溃。

## 2. Loader、应用 API 与物理设备 API

- Loader 版本来自 `vkEnumerateInstanceVersion`，表示本机 Vulkan Loader 能理解的最高 API 版本。
- 应用请求版本写入 `VkApplicationInfo::apiVersion`。本工程将它限制为 Loader 版本与 Vulkan 1.3 两者的较小值，因为本章最高只使用 1.3 核心能力，不无条件请求更高版本。
- 物理设备 API 版本来自 `VkPhysicalDeviceProperties::apiVersion`，表示当前 GPU 驱动为该物理设备实际提供到的 Vulkan 版本。

三者不能混用。Loader 能支持 1.3，不代表任意 GPU 就一定支持 1.3；应用请求 1.3，也不能代替对物理设备特性位的检查。

本机在 Ch6-0 已记录的结果是：Loader 1.4.350、应用请求 1.3.0、NVIDIA GeForce RTX 5070 Ti Laptop GPU 的设备 API 1.4.325、驱动 591.86。

## 3. “支持”与“启用”是两回事

`VKBase.h` 中分别保存两组结构：

- `supportedFeatures2`、`supportedVulkan11Features`、`supportedVulkan12Features`、`supportedVulkan13Features`：由物理设备查询填充，只说明驱动支持什么。
- `enabledFeatures2`、`enabledVulkan11Features`、`enabledVulkan12Features`、`enabledVulkan13Features`：加入 `VkDeviceCreateInfo` 的 `pNext` 链，只说明创建逻辑设备时真正开启了什么。

例如，Ch6-0 的设备同时支持 `imagelessFramebuffer` 和 `dynamicRendering`，但传统模式下两项启用值都为 false。纹理 Sampler 也改为读取 `EnabledFeatures2().features.samplerAnisotropy`，避免“设备支持各向异性”却“逻辑设备没有开启”时仍错误使用它。

## 4. Features2、Properties2 和 MemoryProperties2

`VkPhysicalDeviceFeatures2` 是现代设备特性查询的根结构。它既包含原始 `VkPhysicalDeviceFeatures`，又允许通过 `pNext` 查询 Vulkan 1.1/1.2/1.3 或扩展特性。

`VkPhysicalDeviceProperties2` 同样通过 `pNext` 扩展属性查询。本工程从中读取 GPU 名称、类型、设备 API 版本、驱动名称与驱动信息，并保留旧的 `PhysicalDeviceProperties()` Getter 供现有 Ch7 代码继续使用。

`VkPhysicalDeviceMemoryProperties2` 是内存属性查询根结构。本工程把其 `memoryProperties` 保存回原有成员，因此现有 Buffer/Image 内存类型选择逻辑无需重写。

查询顺序是先取设备属性和 API 版本，再按版本构建特性链：1.4 设备查询 1.1/1.2/1.3 核心结构；1.1 或 1.2 的扩展路径改查对应的 Imageless/Dynamic 扩展特性结构，避免把已提升的聚合结构和扩展结构错误地同时放入一条链。

## 5. `pNext` 链是什么，为什么 Vulkan 大量使用它

Vulkan 结构体通常以 `sType` 和 `pNext` 开头。`pNext` 指向下一个扩展结构，多个结构串成单向链。这样 Vulkan 可以在不改变旧结构体二进制布局的前提下增加新版本或扩展功能，旧代码仍可把不认识的扩展留给驱动处理。

本工程新增的 `pNextChain` 维护链首、结构对象地址集合和 `sType` 集合：

- 同一个结构体对象不能重复加入。
- 默认不允许同一 `sType` 重复加入。
- 根结构的 `sType` 也在对应 `Add...` 接口中单独防重。
- 只有规范明确允许时，调用方才可显式放开同 `sType` 限制。

它被用于 Instance Create Info、Device Create Info、Physical Device Features、Properties、Memory Properties 和 Swapchain Create Info。结构对象必须比使用它的 Vulkan 调用活得更久，因此这些链和核心查询结构都由 `graphicsBase` 持有。

## 6. Imageless Framebuffer 的工作方式

传统路径在 `CreateRpwf_Screen()` 中为每张交换链图像创建一个 Framebuffer，每个对象固定绑定具体 `VkImageView`。

Ch6-1 的 `CreateRpwf_Screen_ImagelessFramebuffer()` 只创建一个 Framebuffer：

- `VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT` 表示创建时不固定附件视图。
- `VkFramebufferAttachmentImageInfo` 描述允许的 usage、宽高、图层数、图像创建 flags 和 view format。
- `VkFramebufferAttachmentsCreateInfo` 把这些附件要求接到 `VkFramebufferCreateInfo::pNext`。
- `VkFramebufferCreateInfo::pAttachments` 为 null，但 `attachmentCount` 仍与 Render Pass 附件数量一致。

每帧开始 Render Pass 时，`renderPass::CmdBeginImageless()` 创建 `VkRenderPassAttachmentBeginInfo`，把当前交换链图像的 `VkImageView` 接到 `VkRenderPassBeginInfo::pNext`。因此一个 Framebuffer 可以适配交换链中的所有图像。

优点是对象更少，附件绑定推迟到命令录制时，适配符合要求的其他图像也更灵活。缺点是仍需维护 Render Pass 和 Framebuffer，对现代渲染器的简化程度有限，而且附件实际兼容性必须在开始 Render Pass 时满足创建阶段声明的要求。

交换链重建时，本工程先销毁这个单一 Framebuffer，再按新 extent、usage、format 和 layer count 重建。Render Pass 在当前工程中保持设备生命周期，因为交换链重建沿用已选 surface format。

## 7. Dynamic Rendering 的工作方式

Dynamic Rendering 是最终默认路径。它不调用 `CreateRpwf_Screen()` 或 `CreateRpwf_Screen_ImagelessFramebuffer()`，所以不会创建任何屏幕 `VkRenderPass` 或 `VkFramebuffer`。

### 管线创建

`CreatePipeline()` 在动态模式下执行以下配置：

- `VkGraphicsPipelineCreateInfo::renderPass = VK_NULL_HANDLE`。
- `VkPipelineRenderingCreateInfo` 接到管线创建信息的 `pNext`。
- `colorAttachmentCount = 1`，颜色格式取当前交换链格式。
- 没有深度和模板附件，所以二者格式都是 `VK_FORMAT_UNDEFINED`。

`VkPipelineRenderingCreateInfo` 的作用，是在没有 Render Pass 对象可提供附件兼容信息时，把管线将要搭配的颜色、深度、模板格式告诉驱动。

### 开始和结束动态渲染

每帧的 `VkRenderingAttachmentInfo` 指定当前交换链 `VkImageView`、`COLOR_ATTACHMENT_OPTIMAL` 布局、clear/load 行为、store 行为和清屏颜色。它描述“这个附件是什么以及如何使用”。

`VkRenderingInfo` 指定 render area、layer count、颜色附件列表以及空的深度/模板附件。它描述“本次动态渲染实例由哪些附件和区域组成”。

`graphicsBase` 根据实际路径加载核心 `vkCmdBeginRendering`/`vkCmdEndRendering`，或 Vulkan 1.2 扩展的 `vkCmdBeginRenderingKHR`/`vkCmdEndRenderingKHR`。入口缺失时会回退传统路径。命令开始和结束之间的绘制代码没有退回三角形，而是继续绑定现有 Pipeline、两路 Vertex Buffer、Index Buffer、Push Constant、Descriptor Set，并调用同一个 `vkCmdDrawIndexed` 完成带纹理的四实例矩形绘制。

## 8. 为什么动态渲染要自己处理布局和屏障

传统 Render Pass 会根据 attachment description、subpass dependency、initial/final layout 隐式执行布局转换和同步。Dynamic Rendering 没有这个对象，因此应用必须显式完成同样的工作。

开始前只转换当前交换链颜色子资源：

- `oldLayout = VK_IMAGE_LAYOUT_UNDEFINED`：本帧会 clear，旧内容不需要保留，明确丢弃即可。
- `newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`。
- `srcStage = TOP_OF_PIPE`、`srcAccess = 0`。
- `dstStage = COLOR_ATTACHMENT_OUTPUT`、`dstAccess = COLOR_ATTACHMENT_WRITE`。
- aspect 为 COLOR，mip 0、level count 1、base layer 0、layer count 取交换链配置。

结束后：

- `oldLayout = COLOR_ATTACHMENT_OPTIMAL`。
- `newLayout = PRESENT_SRC_KHR`。
- `srcStage = COLOR_ATTACHMENT_OUTPUT`、`srcAccess = COLOR_ATTACHMENT_WRITE`。
- `dstStage = BOTTOM_OF_PIPE`、`dstAccess = 0`。

这里继续使用项目已有的传统 `vkCmdPipelineBarrier` 封装，没有与 Synchronization2 混用，也没有每帧调用 `vkDeviceWaitIdle`。

## 9. 三种路径的取舍

| 维度 | Legacy | Imageless | Dynamic |
|---|---|---|---|
| 兼容性 | 最高 | 需 1.2 核心或 KHR 扩展 | 需 1.3 核心或 1.2 KHR 扩展 |
| Render Pass 对象 | 需要 | 需要 | 不需要 |
| Framebuffer 对象 | 每图像一个 | 整个交换链一个 | 不需要 |
| 附件视图绑定时机 | Framebuffer 创建时 | Begin Render Pass 时 | Begin Rendering 时 |
| 布局/同步 | 多数由 Render Pass 描述 | 多数由 Render Pass 描述 | 应用显式屏障 |
| 适用场景 | 教学对照、旧设备、稳定兼容路径 | 学习附件延迟绑定、减少 Framebuffer 数量 | 现代渲染器、减少对象与 Render Pass 组合管理 |

本项目默认 Dynamic Rendering，因为它最直接地减少屏幕渲染对象和交换链重建负担；Legacy 保留为兼容回退和学习对照；Imageless 保留为独立的 Ch6-1 示例，而没有与 Dynamic Rendering 混成一条路径。

## 10. 交换链重建差异

- Legacy：重建交换链图像与 Image View；销毁并重建每图像 Framebuffer；固定 viewport/scissor 的 Pipeline 也重建。
- Imageless：重建交换链图像与 Image View；只销毁并重建一个 Imageless Framebuffer；Pipeline 重建。
- Dynamic：重建交换链图像与 Image View；没有 Render Pass/Framebuffer 要处理；Pipeline 因固定 viewport/scissor 与颜色格式依赖而重建；每帧 render area 直接读取当前 extent。

`graphicsBase::RecreateSwapchain()` 在 extent 为零时返回 `VK_SUBOPTIMAL_KHR`，不会继续创建零尺寸交换链资源。自测代码的最小化循环也不会在窗口最小化期间继续录制绘制命令。

## 11. 实际修改文件与代码结构

- `VKStart.h`：增加现代链管理使用的标准库集合头。
- `helper.h`：让原有 `outStream` 同步输出到标准输出，便于收集能力与 Validation 日志。
- `VKBase.h`：版本查询、Features2/Properties2/MemoryProperties2、`pNextChain`、模式协商、扩展检查、动态渲染入口加载、支持/启用状态日志。
- `MyVulkan.h`：Sampler 改读已启用特性；新增 Imageless Framebuffer 创建和 Begin Render Pass 封装。
- `main.cpp`：三模式参数解析、模式独立资源选择、动态管线信息、动态附件/渲染信息、显式屏障，并保留完整 Ch7 绘制资源。
- `PROGRESS.md`：分节记录实现、命令、设备支持、回退与验证状态。
- `docs/EasyVulkan_Chapter6_Summary.md`：本文。

## 12. 已执行命令与验证边界

Ch6-0 在用户更改验证要求之前实际执行过：

```powershell
D:\VulkanSDK\1.4.350.0\Bin\glslc.exe shader\FirstTriangle.vert.shader -o out\build\vs18-x64-debug\Vulkan-Renderer\shader\FirstTriangle.vert.spv
D:\VulkanSDK\1.4.350.0\Bin\glslc.exe shader\FirstTriangle.frag.shader -o out\build\vs18-x64-debug\Vulkan-Renderer\shader\FirstTriangle.frag.spv
D:\VulkanSDK\1.4.350.0\Bin\spirv-val.exe <两个 SPIR-V 文件>
cmake --build --preset vs18-x64-debug --config Debug
$env:VK_INSTANCE_LAYERS='VK_LAYER_KHRONOS_validation'
.\Debug\Vulkan-Renderer.exe --self-test
```

该次 Legacy 运行退出码为 0，自测日志包含 960x540 缩放、最小化、恢复和正常关闭；日志中 `ERROR`、`VUID`、`WARNING`、`Validation Error` 的匹配数为 0。

根据用户随后“之后的所有修改都不需要你进行验证，我亲自来验证”的要求，Ch6-1、Ch6-2 以及最终整合后没有再次编译 Shader、构建程序、启动应用或运行 Validation。它们的状态必须理解为“代码已实现，待用户验证”，不能把 Ch6-0 的结果推断成后两种新路径已经通过。

建议用户分别执行：

```powershell
.\Debug\Vulkan-Renderer.exe --mode=legacy
.\Debug\Vulkan-Renderer.exe --mode=imageless
.\Debug\Vulkan-Renderer.exe --mode=dynamic
```

并在 Debug x64 + Khronos Validation 下分别检查画面、缩放、最小化/恢复、退出资源销毁，以及所有 `ERROR`、`VUID` 和相关 `WARNING`。
