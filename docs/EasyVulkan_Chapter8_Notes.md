# EasyVulkan 第八章学习笔记

本笔记对应 `Vulkan-Renderer` 当前工程中的实际代码。第八章没有替换第六章的 Legacy/Imageless/Dynamic Rendering，也没有删除第七章的 Vertex、Index、Instance、Push Constant、Uniform、Descriptor、Texture 和 Mipmap 功能，而是在独立的 `Chapter8.cpp` 中增加多阶段渲染示例。

第八章示例统一通过命令行选择：

```text
--example=offscreen
--example=depth
--example=depth-off
--example=depth-raw
--example=depth-linear
--example=deferred
--example=gbuffer-albedo
--example=gbuffer-normal
--example=gbuffer-position
--example=alpha
--example=srgb
--example=sdr
--example=hdr
```

HDR 偏好通过下列参数选择：

```text
--hdr=auto
--hdr=sdr
--hdr=request
```

第八章示例优先使用传统 Render Pass，以便保留 EasyVulkan 原章节中的 Attachment、Subpass 和 Input Attachment 结构；没有参数时仍进入第六、七章原有的 Forward/Dynamic Rendering 路径。

> 验证边界：根据用户要求，本次第八章修改没有编译 Shader、没有构建 C++、没有启动程序，也没有运行 Validation Layer。文中的“已实现”表示代码路径已经写入工程，不表示画面或设备能力已经通过运行验证。

---

## 第一部分：离屏渲染

### 1. 什么是离屏渲染

直接渲染时，Fragment Shader 最终写入交换链图像。离屏渲染则先写入应用自己创建的普通 `VkImage`，之后再采样、复制或继续处理这张图像。

当前工程的 Ch8-1 数据流是：

```text
程序化彩色场景
→ Offscreen Render Pass
→ R8G8B8A8_UNORM 离屏颜色 Image
→ Image View
→ Combined Image Sampler Descriptor
→ Fullscreen Triangle
→ Screen Render Pass
→ Swapchain Image
```

屏幕阶段不会重新绘制场景几何，只执行一次全屏三角形采样，因此屏幕内容必然来自离屏图像。

### 2. 为什么不总是直接渲染到交换链

离屏图像可用于：

- 后处理和 Tone Mapping；
- 深度或 G-buffer 调试显示；
- 多 Pass 渲染；
- 缓存长期使用的渲染结果；
- HDR 中间颜色；
- 不依赖窗口交换链的图像生成。

交换链图像由窗口系统管理，数量、尺寸和可用时间受 WSI 控制。应用自己持有的离屏图像更适合在多个阶段间传递。

### 3. Image、Memory、Image View、Framebuffer 和 Sampler

这些对象的关系如下：

```text
VkDeviceMemory
└─ 绑定到 VkImage，提供真实存储
   ├─ VkImageView：规定格式、Aspect、Mip 和 Layer 的观察方式
   │  ├─ 作为 VkFramebuffer Attachment
   │  └─ 写入 Descriptor，供 Shader 采样
   └─ VkSampler：规定过滤、寻址、LOD 等采样行为
```

`imageMemory` 按以下顺序创建和销毁：

```text
创建：Image → Allocate Memory → Bind Memory → Image View
销毁：Image View → Image → Device Memory
```

Framebuffer 不拥有 Image View，只引用它，所以 Framebuffer 必须先销毁。

### 4. 离屏颜色附件 Usage

Ch8-1 只需要两种真实用途：

```cpp
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
VK_IMAGE_USAGE_SAMPLED_BIT
```

没有执行 Copy、Blit 或手动 Clear，所以没有加入无用的 Transfer Usage。创建前还会检查：

```cpp
VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT
```

### 5. 布局和同步

离屏 Render Pass 中：

```text
Initial Layout  = UNDEFINED
Subpass Layout  = COLOR_ATTACHMENT_OPTIMAL
Final Layout    = SHADER_READ_ONLY_OPTIMAL
Load Op         = CLEAR
Store Op        = STORE
```

子通道结束依赖为：

```text
COLOR_ATTACHMENT_OUTPUT / COLOR_ATTACHMENT_WRITE
→ FRAGMENT_SHADER / SHADER_READ
```

这使离屏颜色写入在随后全屏 Fragment Shader 采样时可见。两个 Render Pass 被录制在同一命令缓冲区，没有插入每帧 `vkDeviceWaitIdle`。

### 6. 多帧冲突

当前项目保持一个 Frame In Flight：每帧末尾等待对应 Fence 后，下一帧才复用命令缓冲区、Uniform 和离屏附件。因此同一张离屏图像不会被前一帧读取、当前帧同时写入。

如果以后增加两个或三个并行帧，应为每个 Frame In Flight 创建独立 Offscreen Target，或重新设计跨帧同步，不能直接共享一张频繁读写的图像。

### 7. 交换链重建

离屏目标尺寸跟随 `windowSize`。交换链回调会重建：

- 离屏 Image、Memory 和 View；
- 离屏 Framebuffer；
- Descriptor Pool、Descriptor Set 和 Image Info；
- 屏幕 Framebuffer；
- Render Pass；
- 固定 Viewport/Scissor 的 Pipeline。

---

## 第二部分：深度测试

### 1. 深度测试解决什么问题

三角形提交顺序不能代表空间遮挡关系。深度测试为每个片段保存深度值，并比较新片段与已有值，决定片段是否继续写入颜色和深度附件。

当前示例绘制三个位置、距离和旋转不同的立方体。它们共享一个 Vertex Buffer、Index Buffer 和一次实例化 `vkCmdDrawIndexed`，但在 UBO 中拥有三个 Model Matrix。

### 2. 深度格式选择

工程依次检查：

```cpp
VK_FORMAT_D32_SFLOAT
VK_FORMAT_D16_UNORM
```

必须同时具备：

```cpp
VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
```

这两个候选都是纯深度格式，因此 View 使用：

```cpp
VK_IMAGE_ASPECT_DEPTH_BIT
```

如果未来选择 `D32_SFLOAT_S8_UINT` 或 `D24_UNORM_S8_UINT`，用于深度/模板附件的 View 需要按真实用途考虑 `VK_IMAGE_ASPECT_STENCIL_BIT`；采样深度时则通常创建独立的 Depth-only View。

### 3. 深度附件创建

为了同时用于深度测试和全屏可视化，Usage 为：

```cpp
VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
VK_IMAGE_USAGE_SAMPLED_BIT
```

Render Pass 的颜色和深度附件最终分别进入：

```text
Color → SHADER_READ_ONLY_OPTIMAL
Depth → DEPTH_STENCIL_READ_ONLY_OPTIMAL
```

### 4. Pipeline Depth State

`graphicsPipelineCreateInfoPack` 新增 `VkPipelineDepthStencilStateCreateInfo`。正常深度配置为：

```cpp
depthTestEnable  = VK_TRUE;
depthWriteEnable = VK_TRUE;
depthCompareOp   = VK_COMPARE_OP_LESS;
```

清除值为：

```cpp
VkClearDepthStencilValue{ 1.0f, 0 }
```

普通深度中，Near 对应更小值，Far 接近 1，所以使用 `LESS`。如果改成 Reverse-Z，必须同时改变投影、清除值和比较函数，不能只改其中一个。

Depth Test 与 Depth Write 不同：

- Depth Test 决定是否用已有深度拒绝新片段；
- Depth Write 决定通过测试的片段是否更新深度缓冲。

透明物体常见配置是保留 Depth Test、关闭 Depth Write，但仍需要排序。

### 5. 面剔除和绕序

立方体管线使用：

```cpp
cullMode = VK_CULL_MODE_BACK_BIT;
frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
```

CPU 投影矩阵执行 Vulkan Y 翻转，顶点索引按外表面绕序组织。正常模式没有永久关闭面剔除来掩盖模型绕序问题。

### 6. 为什么透视深度是非线性的

深度缓冲保存的是投影变换和透视除法后的窗口空间深度，不是世界空间距离。透视投影把更多精度分配在 Near Plane 附近，所以 Near 设置过小会严重浪费远处精度。

当前 Shader 使用 Vulkan `[0,1]` 深度范围的线性化公式：

```glsl
float LinearizeDepth(float depth, float nearPlane, float farPlane) {
    return nearPlane * farPlane /
        (farPlane - depth * (farPlane - nearPlane));
}
```

Near/Far 在 CPU 和 Shader 中统一为：

```text
Near = 0.1
Far  = 50.0
```

模式对应：

```text
--example=depth        彩色场景
--example=depth-off    同一场景关闭 Depth Test/Write 的错误对照
--example=depth-raw    原始非线性深度
--example=depth-linear 线性化并归一化的深度
```

线性视图额外使用幂函数扩展较暗的 Near 区域，仅用于调试显示，不会改变深度缓冲本身。

---

## 第三部分：延迟渲染

### 1. Forward 与 Deferred

Forward Rendering：

```text
每个几何片段
→ 立即计算光照
→ 深度测试后可能被遮挡
→ 输出颜色
```

Deferred Rendering：

```text
Geometry Pass
→ 只写几何/材质信息到 G-buffer
→ 深度测试保留可见表面

Composition Pass
→ 每个屏幕像素读取 G-buffer
→ 只对最终可见数据计算光照
→ 输出交换链
```

光照被“延迟”到几何可见性确定之后，因此大量光源或复杂几何时可以减少被遮挡片段上的重复光照计算。

### 2. 当前 G-buffer

工程只创建实际使用的附件：

| 附件 | 格式 | 内容 |
|---|---|---|
| Albedo | `R8G8B8A8_UNORM` | 线性基础颜色 |
| Normal | `R16G16B16A16_SFLOAT` | 世界空间单位法线 |
| Position | `R16G16B16A16_SFLOAT` | 世界空间位置 |
| Depth | 运行时选择 | 深度测试和背景判断 |

Normal 和 Position 是数据，不能使用 SRGB 格式。Geometry Vertex Shader 使用：

```glsl
mat3(transpose(inverse(model)))
```

作为 Normal Matrix，因此非等比缩放时仍能得到正确法线方向。

### 3. 两个 Subpass

传统 Render Pass 结构为：

```text
Subpass 0：Geometry
  Color Attachments = Albedo, Normal, Position
  Depth Attachment  = Depth

Subpass 1：Composition
  Input Attachments = Albedo, Normal, Position, Depth
  Color Attachment  = Swapchain
```

Geometry Pass 使用带深度测试的立方体 Pipeline。Composition Pass 使用 `gl_VertexIndex` 生成全屏三角形，不需要屏幕 Vertex Buffer。

### 4. Input Attachment 对应关系

Descriptor Set Layout 有四个 `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT` Binding：

```text
binding 0 ↔ input_attachment_index 0 ↔ Albedo
binding 1 ↔ input_attachment_index 1 ↔ Normal
binding 2 ↔ input_attachment_index 2 ↔ Position
binding 3 ↔ input_attachment_index 3 ↔ Depth
```

Shader 使用 `subpassInput` 和 `subpassLoad`。Input Attachment 只读取当前片段位置，不是普通任意 UV 采样，因此 tile-based GPU 有机会把数据保留在片上存储。

### 5. Subpass Dependency

最关键的依赖是：

```text
srcSubpass  = Geometry 0
dstSubpass  = Composition 1
srcStage    = COLOR_ATTACHMENT_OUTPUT | LATE_FRAGMENT_TESTS
dstStage    = FRAGMENT_SHADER
srcAccess   = COLOR_ATTACHMENT_WRITE | DEPTH_STENCIL_ATTACHMENT_WRITE
dstAccess   = INPUT_ATTACHMENT_READ
flags       = BY_REGION
```

这保证 Geometry 的颜色和深度写入对 Composition 的 Input Attachment 读取可见，没有使用宽泛的 `ALL_COMMANDS` 掩码。

### 6. 光照与调试模式

Composition 只实现环境光和一个方向光漫反射：

```text
Final = Ambient * Albedo
      + max(dot(Normal, LightDirection), 0) * Albedo
```

调试模式：

```text
--example=deferred          最终光照
--example=gbuffer-albedo    Albedo
--example=gbuffer-normal    Normal 映射到 [0,1]
--example=gbuffer-position  Position 压缩显示
```

### 7. 延迟渲染限制

- G-buffer 增加显存占用和写入/读取带宽；
- 普通延迟渲染只保留最前表面，无法直接获得透明物体后面的颜色；
- 半透明通常需要在延迟阶段后增加 Forward Transparency Pass；
- MSAA 会让每像素拥有多份 G-buffer 样本，解析和光照更复杂；
- 移动/tile GPU 上，带宽、片上内存和 Render Pass 结构会影响是否真的优于 Forward；
- 当前示例没有 PBR、阴影或复杂材质系统。

---

## 第四部分：预乘 Alpha

### 1. 两种表示

Straight Alpha：

```text
RGB = 原始颜色
A   = 透明度
```

Premultiplied Alpha：

```text
RGB = 原始颜色 × A
A   = 透明度
```

例如原始红色 `(1,0,0)`、Alpha 为 `0.25`：

```text
Straight      = (1.00, 0, 0, 0.25)
Premultiplied = (0.25, 0, 0, 0.25)
```

### 2. 为什么透明边缘会出现黑边

线性过滤和 Mipmap 会在相邻像素之间插值。如果完全透明像素仍保存黑色 RGB，Straight Alpha 的 RGB 插值可能先混入黑色，之后再乘 Alpha，于是边缘变暗。

Premultiplied Alpha 让 RGB 已经包含覆盖率，透明像素趋近零，过滤和缩小时更符合合成数学。

### 3. PNG 转换

PNG/stb 通常返回 Straight Alpha。`texture2d::Create` 新增 `premultiplyAlpha` 参数，并在上传前转换：

```cpp
premultipliedRGB = (uint16_t(RGB) * A + 127) / 255;
```

使用 16 位中间值避免 8 位乘法溢出，`+127` 提供接近四舍五入的整数结果。

正确顺序是：

```text
读取 Straight PNG
→ CPU 预乘 RGB
→ 上传 Mip 0
→ GPU Blit 生成后续 Mipmap
```

不能先用 Straight 数据生成所有 Mip，再只修改 Mip 0。

### 4. Blend State

Straight Alpha：

```cpp
srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
```

Premultiplied Alpha：

```cpp
srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
```

两者 Alpha 通道都使用：

```cpp
srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
```

### 5. 当前三面板对比

`--example=alpha` 同时绘制：

```text
左：Straight 纹理 + Straight Blend，正确
中：Straight 纹理 + Premultiplied Blend，故意错误
右：Premultiplied 纹理 + Premultiplied Blend，正确
```

Premultiplied Fragment Shader 不再执行 `rgb *= alpha`，否则会重复预乘。

预乘 Alpha 不能替代透明排序。多个半透明表面相互覆盖时，常规 Alpha 混合仍通常需要从后向前绘制；预乘解决的是颜色表示、过滤和混合公式，不是可见性排序。

---

## 第五部分：sRGB

### 1. 线性空间与 sRGB 编码

线性空间中，数值与物理光强近似成比例；sRGB 是为显示和存储设计的非线性编码，在暗部使用更多编码精度。

不能直接在 sRGB 编码值上进行光照、颜色相加或插值。例如黑色和白色的物理平均在线性空间是 `0.5`，编码显示约为 `0.735`；直接平均编码端点得到 `0.5`，视觉上明显偏暗。

### 2. UNORM 与 SRGB 格式

`R8G8B8A8_UNORM`：

- 8 位整数映射到 `[0,1]`；
- 采样和写入不自动进行颜色变换；
- 适合 Mask、AO 或只借用数值范围的线性数据。

`R8G8B8A8_SRGB`：

- 采样时，硬件自动把 RGB 从 sRGB 解码到线性；
- 写入 SRGB Attachment 时，硬件自动把线性 RGB 编码为 sRGB；
- Alpha 不进行 sRGB 转换。

### 3. 颜色贴图与数据贴图

颜色贴图通常使用 SRGB：

```text
Albedo / Base Color
普通彩色 UI 或照片
当前 Ch7 方向测试纹理和启动图片
```

数据贴图必须保持非 SRGB：

```text
Normal
Roughness
Metallic
AO
Mask
Depth
Position
```

对 Normal 做 sRGB 解码会改变向量分量，破坏方向。

### 4. 当前工程颜色数据流

SDR 路径优先选择 SRGB Swapchain Attachment：

```text
SRGB 颜色纹理
→ 采样时自动解码为 Linear
→ Shader 在线性空间执行颜色运算/光照
→ 写入 SRGB Swapchain Attachment
→ 硬件进行一次 sRGB 编码
```

如果 Surface 只有 UNORM 格式，Tone Mapping Shader 会手动执行一次 Linear-to-sRGB；如果 Attachment 已是 SRGB，Shader 不再手动编码，避免重复 Gamma。

### 5. sRGB 对比画面

`--example=srgb` 将画面分成两半：

- 左侧显示错误的编码空间 50%；
- 右侧显示正确的线性空间 50%；
- 顶部黑白条提供端点参考。

Shader 根据 Swapchain Format 判断编码由 SRGB Attachment 自动完成，还是需要在 UNORM 输出前手动完成。

---

## 第六部分：HDR

### 1. HDR 中间渲染目标

HDR 不是简单更换交换链格式。场景首先写入：

```cpp
VK_FORMAT_R16G16B16A16_SFLOAT
```

Usage 为：

```cpp
VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
VK_IMAGE_USAGE_SAMPLED_BIT
```

浮点附件能够保存大于 `1.0` 的场景线性值。`HDRScene.frag.shader` 生成亮度渐变和最高约 8.0 的高亮区域，证明中间目标不是 0～1 的 LDR 缓冲。

### 2. Tone Mapping

Tone Mapping 把场景 HDR 动态范围压缩到显示输出范围。SDR 默认使用 ACES 近似：

```glsl
color = clamp(
    (x * (2.51 * x + 0.03)) /
    (x * (2.43 * x + 0.59) + 0.14),
    0.0, 1.0);
```

SDR 数据流：

```text
HDR Scene Linear
→ Exposure
→ ACES Approx Tone Mapping
→ SDR Linear
→ SRGB Attachment 自动编码，或 UNORM 前手动编码一次
→ SDR Swapchain
```

### 3. HDR 输出候选

程序只在 `VK_EXT_swapchain_colorspace` 被实例扩展查询确认后启用它，并枚举实际 Surface Format/Color Space 组合。

支持两类候选：

```text
scRGB：R16G16B16A16_SFLOAT + EXTENDED_SRGB_LINEAR
HDR10：A2B10G10R10_UNORM 或 A2R10G10B10_UNORM + HDR10_ST2084
```

scRGB 路径写入线性浮点值；HDR10 路径执行：

```text
Scene Linear Rec.709
→ Rec.2020 Primaries
→ 以 1.0 = 100 nit 标定
→ ST.2084 PQ 编码
→ HDR10 Swapchain
```

### 4. Auto、Force SDR、Request HDR

```text
--hdr=auto
  --example=hdr 时尝试 HDR，其他示例保持 SDR。

--hdr=sdr
  强制 SDR Tone Mapping，即使系统枚举出 HDR 候选。

--hdr=request
  明确请求 HDR；找不到候选时记录日志并安全回退 SDR。
```

### 5. 为什么选择 HDR Swapchain 仍不等于 HDR 正确

完整 HDR 输出还依赖：

- 显示器真实峰值亮度和色域；
- Windows HDR 设置；
- 显卡驱动；
- 窗口系统暴露的 Surface Format；
- Color Space；
- 合成器是否保持 HDR；
- 内容的参考白、曝光和最大亮度标定。

仅看到“画面更亮”不能证明 PQ、色域和显示链正确。

### 6. 当前设备能力与验证状态

此前 Ch6-0 运行记录显示物理设备为 NVIDIA GeForce RTX 5070 Ti Laptop GPU，Loader 与设备支持现代 Vulkan；但本次没有运行 Ch8-5，因此没有实际记录当前显示器和 Windows 状态下的 HDR Surface Format/Color Space。

已写入代码但尚未运行确认：

- `VK_EXT_swapchain_colorspace` 是否在当前实例环境可用；
- 是否枚举到 scRGB 或 HDR10 组合；
- SDR/SRGB 对比画面；
- HDR 浮点中间目标；
- SDR Tone Mapping；
- HDR 实际输出；
- HDR 不支持时的运行回退日志。

因此不能声称当前设备的 HDR 输出已经通过验证。

---

## 第七部分：整体数据流程

完整的学习流程图：

```text
场景几何 / 程序化场景
→ Geometry 或 Forward Pass
→ 普通离屏颜色 / HDR 浮点颜色
→ Depth Attachment
→ G-buffer：Albedo + Normal + Position + Depth
→ Deferred Lighting / Fullscreen Composition
→ Straight 或 Premultiplied Alpha 混合
→ Tone Mapping
→ SDR sRGB 编码 或 scRGB / HDR10 PQ 输出
→ Swapchain
→ 显示器
```

每个示例只启用理解当前主题所需的部分，没有引入 Render Graph、ECS、PBR、阴影、Bloom、TAA、模型加载器或后续章节功能。

---

## 第八部分：项目文件结构

### 1. 新增核心文件

```text
Chapter8.h
  RenderExample / HdrPreference
  参数解析与第八章入口声明

Chapter8.cpp
  Offscreen Target
  Depth Attachment
  G-buffer
  Alpha 纹理与管线
  HDR Target
  Render Pass / Subpass / Framebuffer
  Descriptor / Pipeline
  交换链重建回调
  第八章渲染循环
```

### 2. Shader

```text
Offscreen.vert/frag               离屏程序化场景
Fullscreen.vert/frag              全屏三角形与颜色采样
DepthScene.vert/frag              三维立方体和基础光照
DepthVisualize.frag               原始/线性深度
DeferredGeometry.vert/frag        G-buffer 写入
DeferredComposition.frag          Input Attachment 与光照/调试
AlphaTest.vert/frag               三面板 Alpha 对比
ColorSpaceTest.frag               Gamma/Linear 对比
HDRScene.frag                     大于 1.0 的 HDR 场景
ToneMapping.frag                  SDR/scRGB/HDR10 输出
```

CMake 使用 `CONFIGURE_DEPENDS` 自动发现 `shader/*.vert.shader` 和 `shader/*.frag.shader`，并把：

```text
Name.vert.shader → Name.vert.spv
Name.frag.shader → Name.frag.spv
```

### 3. 修改的公共封装

`MyVulkan.h`：

- Graphics Pipeline Pack 增加 Depth/Stencil State；
- `texture2d::Create` 增加可选 CPU Alpha 预乘；
- 仍保持 View → Image → Memory 的明确销毁顺序。

`VKBase.h`：

- SDR Surface Format 默认优先 SRGB；
- HDR 仍通过现有 Surface Format 查询和 `SetSurfaceFormat` 选择，不硬编码为必定支持。

`main.cpp`：

- 解析 `--example` 与 `--hdr`；
- 第八章示例请求 Legacy Render Pass；
- Forward 模式继续保留 Ch6 三路径；
- HDR 扩展仅在实例扩展查询确认后启用。

### 4. 资源生命周期

交换链相关资源销毁顺序概括为：

```text
Framebuffer
→ Pipeline
→ Render Pass
→ Descriptor Pool/Set
→ Image View
→ Image
→ Device Memory
```

设备生命周期资源在所有 Pipeline 销毁后处理：

```text
Shader Module
→ Buffer/Texture
→ Sampler
→ Pipeline Layout
→ Descriptor Set Layout
```

交换链回调只重建尺寸、格式或 Image View 相关资源。窗口最小化时，渲染循环等待事件，不继续创建零尺寸附件。

---

## 第九部分：本章总结

第七章解决了“如何把资源送入一次绘制”：

```text
Vertex / Index / Instance
Push Constant / UBO / Descriptor
Texture / Sampler / Mipmap
```

第八章开始解决“如何组织多阶段渲染结果”：

```text
离屏渲染       把交换链之外的 Image 变成中间结果
深度遮挡       用 Depth Attachment 决定可见性
深度可视化     把深度作为数据重新读取
延迟光照       用 G-buffer 和 Subpass 延后光照
正确透明混合   区分 Straight 与 Premultiplied Alpha
正确颜色空间   区分 Linear、UNORM 和 SRGB
SDR/HDR 输出   用浮点中间颜色、Tone Mapping 和 Color Space 管理显示输出
```

这使工程从“带纹理的单阶段绘制示例”扩展成了具有离屏附件、深度、G-buffer、Composition、Alpha 合成和颜色输出管理的基础多阶段渲染器，同时仍保持教程规模和显式 Vulkan 对象关系。
