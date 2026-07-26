#include "GLfwGeneral.hpp"
#include "MyVulkan.h"
#include "RenderExample.h"
#include <cstddef>
#include <cmath>
#include <array>
#include <thread>
#include <string_view>

using namespace vulkan;
using namespace easyVulkan;

void TitleFps();

struct Vertex
{
	glm::vec2 position;
	glm::vec4 color;
	glm::vec2 texCoord;
};

struct InstanceData
{
	glm::vec2 offset;
	glm::vec4 color;
};

struct alignas(16) PushConstantData
{
	alignas(16) glm::vec4 color;
	alignas(8) glm::vec2 scale;
	glm::vec2 padding;
};

static_assert(sizeof(PushConstantData) == 32);

struct alignas(16) UniformData
{
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
};

static_assert(sizeof(UniformData) == sizeof(glm::mat4) * 3);

const Vertex vertices_rectangle[] =
{
	{ { -0.2f, -0.2f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f } },
	{ {  0.2f, -0.2f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f } },
	{ { -0.2f,  0.2f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f } },
	{ {  0.2f,  0.2f }, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f } }
};

const uint16_t indices_rectangle[] =
{
	0, 1, 2,
	1, 3, 2
};

const InstanceData instances_rectangle[] =
{
	{ { -0.45f, -0.45f }, { 1.0f, 0.25f, 0.25f, 1.0f } },
	{ {  0.45f, -0.45f }, { 0.25f, 1.0f, 0.25f, 1.0f } },
	{ { -0.45f,  0.45f }, { 0.25f, 0.45f, 1.0f, 1.0f } },
	{ {  0.45f,  0.45f }, { 1.0f, 0.85f, 0.2f, 1.0f } }
};

// VkPipelineLayout: 组合场景描述符集布局与顶点阶段 Push Constant 范围。
pipelineLayout pipelineLayout_triangle;
descriptorSetLayout descriptorSetLayout_scene;
// VkPipeline: 带纹理的索引实例化绘制所使用的图形管线。
pipeline pipeline_triangle;

const auto& LegacyRenderPassAndFramebuffers()
{
	static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
	return rpwf;
}

const auto& ImagelessRenderPassAndFramebuffer()
{
	static const auto& rpwf = easyVulkan::CreateRpwf_Screen_ImagelessFramebuffer();
	return rpwf;
}

VkRenderPass ActiveRenderPass()
{
	if (graphicsBase::Base().ActiveRenderMode() == RenderMode::ImagelessFramebuffer)
		return ImagelessRenderPassAndFramebuffer().renderPass;
	return LegacyRenderPassAndFramebuffers().renderPass;
}

void CreateLayout()
{
	if (sizeof(PushConstantData) > graphicsBase::Base().PhysicalDeviceProperties().limits.maxPushConstantsSize)
	{
		outStream << "[ CreateLayout ] ERROR\nPush constant data exceeds maxPushConstantsSize!\n";
		abort();
	}
	VkPushConstantRange pushConstantRange =
	{
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(PushConstantData)
	};
	VkDescriptorSetLayoutBinding descriptorBindings[] =
	{
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		}
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo =
	{
		.bindingCount = uint32_t(std::size(descriptorBindings)),
		.pBindings = descriptorBindings
	};
	if (descriptorSetLayout_scene.Create(descriptorSetLayoutCreateInfo))
		abort();
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
	{
		.setLayoutCount = 1,
		.pSetLayouts = descriptorSetLayout_scene.Address(),
		.pushConstantRangeCount = 1,
		.pPushConstantRanges = &pushConstantRange
	};
	pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
	static bool callbackAdded = false;
	if (!callbackAdded)
	{
		graphicsBase::Base().AddCallback_DestroyDevice([] {
			pipelineLayout_triangle.Destroy();
			descriptorSetLayout_scene.Destroy();
		});
		callbackAdded = true;
	}
}

void CreatePipeline()
{
	static shaderModule vert("shader/FirstTriangle.vert.spv");
	static shaderModule frag("shader/FirstTriangle.frag.spv");
	// VkPipelineShaderStageCreateInfo: 分别描述顶点阶段和片段阶段使用的 shader module。
	static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[2] =
	{
		vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
		frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	auto Create = [] {
		graphicsPipelineCreateInfoPack pipelineCiPack;
		pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
		VkFormat colorAttachmentFormat = graphicsBase::Base().SwapchainCreateInfo().imageFormat;
		VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &colorAttachmentFormat,
			.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
			.stencilAttachmentFormat = VK_FORMAT_UNDEFINED
		};
		if (graphicsBase::Base().ActiveRenderMode() == RenderMode::DynamicRendering)
		{
			pipelineCiPack.createInfo.pNext = &pipelineRenderingCreateInfo;
			pipelineCiPack.createInfo.renderPass = VK_NULL_HANDLE;
		}
		else
			pipelineCiPack.createInfo.renderPass = ActiveRenderPass();
		pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		pipelineCiPack.vertexInputBindings.push_back(
			{
				.binding = 0,
				.stride = sizeof(Vertex),
				.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
			});
		pipelineCiPack.vertexInputAttributes.push_back(
			{
				.location = 0,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(Vertex, position)
			});
		pipelineCiPack.vertexInputAttributes.push_back(
			{
				.location = 1,
				.binding = 0,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = offsetof(Vertex, color)
			});
		pipelineCiPack.vertexInputBindings.push_back(
			{
				.binding = 1,
				.stride = sizeof(InstanceData),
				.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE
			});
		pipelineCiPack.vertexInputAttributes.push_back(
			{
				.location = 2,
				.binding = 0,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(Vertex, texCoord)
			});
		pipelineCiPack.vertexInputAttributes.push_back(
			{
				.location = 3,
				.binding = 1,
				.format = VK_FORMAT_R32G32_SFLOAT,
				.offset = offsetof(InstanceData, offset)
			});
		pipelineCiPack.vertexInputAttributes.push_back(
			{
				.location = 4,
				.binding = 1,
				.format = VK_FORMAT_R32G32B32A32_SFLOAT,
				.offset = offsetof(InstanceData, color)
			});

		// VkViewport: 让标准化设备坐标覆盖整个交换链图像。
		pipelineCiPack.viewports.push_back({ 0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f });
		// VkRect2D: 裁剪范围也覆盖整个交换链图像。
		pipelineCiPack.scissors.push_back({ {}, windowSize });
		pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		// VkPipelineColorBlendAttachmentState: 不开启混合，只允许写入 RGBA 四个颜色通道。
		pipelineCiPack.colorBlendAttachmentStates.push_back(
			{
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
					VK_COLOR_COMPONENT_G_BIT |
					VK_COLOR_COMPONENT_B_BIT |
					VK_COLOR_COMPONENT_A_BIT
			});
		pipelineCiPack.UpdateAllArrays();
		pipelineCiPack.createInfo.stageCount = 2;
		pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangle;
		pipeline_triangle.Create(pipelineCiPack);
	};

	auto Destroy = [] {
		pipeline_triangle.Destroy();
	};

	static bool callbacksAdded = false;
	if (!callbacksAdded)
	{
		graphicsBase::Base().AddCallback_CreateSwapchain(Create);
		graphicsBase::Base().AddCallback_DestroySwapchain(Destroy);
		graphicsBase::Base().AddCallback_DestroyDevice([] {
			vert.Destroy();
			frag.Destroy();
		});
		callbacksAdded = true;
	}
	Create();
}

VkResult ShowBootImage(const char* filepath)
{
	int width = 0;
	int height = 0;
	int channelCount = 0;
	stbi_uc* pixels = stbi_load(filepath, &width, &height, &channelCount, STBI_rgb_alpha);
	if (!pixels || width <= 0 || height <= 0)
	{
		outStream << std::format("[ ShowBootImage ] ERROR\nFailed to load image: {}\n", filepath);
		if (pixels)
			stbi_image_free(pixels);
		return VK_ERROR_INITIALIZATION_FAILED;
	}

	const VkFormat sourceFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkFormatProperties sourceFormatProperties;
	VkFormatProperties destinationFormatProperties;
	vkGetPhysicalDeviceFormatProperties(graphicsBase::Base().PhysicalDevice(), sourceFormat, &sourceFormatProperties);
	vkGetPhysicalDeviceFormatProperties(graphicsBase::Base().PhysicalDevice(),
		graphicsBase::Base().SwapchainCreateInfo().imageFormat, &destinationFormatProperties);
	if (!(sourceFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) ||
		!(sourceFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) ||
		!(destinationFormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) ||
		!(graphicsBase::Base().SwapchainCreateInfo().imageUsage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		outStream << "[ ShowBootImage ] ERROR\nRequired linear blit features are not supported!\n";
		stbi_image_free(pixels);
		return VK_ERROR_FORMAT_NOT_SUPPORTED;
	}

	const VkDeviceSize imageSize = VkDeviceSize(width) * VkDeviceSize(height) * STBI_rgb_alpha;
	bufferMemory stagingBuffer;
	VkResult result = stagingBuffer.CreateHostVisible(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
	if (!result)
		result = stagingBuffer.Write(pixels, imageSize);
	stbi_image_free(pixels);
	if (result)
		return result;

	imageMemory sourceImage;
	const VkExtent3D sourceExtent = { uint32_t(width), uint32_t(height), 1 };
	result = sourceImage.Create(sourceExtent, 1, sourceFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	if (!result)
		result = ExecuteGraphicsCommands([&](VkCommandBuffer commandBuffer) {
			const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
			imageOperation::CmdTransitionLayout(commandBuffer, sourceImage,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				0, VK_ACCESS_TRANSFER_WRITE_BIT, range);
			imageOperation::CmdCopyBufferToImage(commandBuffer, stagingBuffer, sourceImage, sourceExtent);
			imageOperation::CmdTransitionLayout(commandBuffer, sourceImage,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, range);
		});
	if (result)
		return result;

	semaphore imageAvailable;
	semaphore renderingFinished;
	fence transferFinished;
	commandPool transferCommandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
	commandBuffer transferCommandBuffer;
	if (transferCommandPool.AllocateBuffers(transferCommandBuffer))
		return VK_ERROR_INITIALIZATION_FAILED;
	if ((result = graphicsBase::Base().SwapImage(imageAvailable)))
		return result;
	if ((result = transferCommandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)))
		return result;

	const VkImage swapchainImage = graphicsBase::Base().SwapchainImage(graphicsBase::Base().CurrentImageIndex());
	const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	imageOperation::CmdTransitionLayout(transferCommandBuffer, swapchainImage,
		VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		0, VK_ACCESS_TRANSFER_WRITE_BIT, range);
	imageOperation::CmdBlitImage(transferCommandBuffer, sourceImage, swapchainImage,
		{ width, height, 1 },
		{ int32_t(windowSize.width), int32_t(windowSize.height), 1 });
	imageOperation::CmdTransitionLayout(transferCommandBuffer, swapchainImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		VK_ACCESS_TRANSFER_WRITE_BIT, 0, range);
	if ((result = transferCommandBuffer.End()))
		return result;
	if ((result = graphicsBase::Base().SubmitCommandBuffer_Graphics(transferCommandBuffer,
		imageAvailable, renderingFinished, transferFinished, VK_PIPELINE_STAGE_TRANSFER_BIT)))
		return result;
	if ((result = graphicsBase::Base().PresentImage(renderingFinished)))
		return result;
	if ((result = transferFinished.WaitAndReset()))
		return result;
	// 呈现操作不受 transferFinished 栅栏保护；启动图只执行一次，此处等待呈现队列后
	// 再销毁局部二值信号量，避免信号量仍被 WSI 使用。
	if ((result = vkQueueWaitIdle(graphicsBase::Base().Queue_Presentation())))
		return result;

	outStream << std::format("[ Ch7-6 ] Blitted {}x{} image to {}x{} swapchain image.\n",
		width, height, windowSize.width, windowSize.height);
	return VK_SUCCESS;
}

int Run(bool selfTest)
{
	if (ShowBootImage("textures/viking_room.png"))
		return -1;
	const double bootImageEndTime = glfwGetTime() + 2.0;
	while (!glfwWindowShouldClose(pWindow) && glfwGetTime() < bootImageEndTime)
	{
		glfwPollEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
	}

	const bool useImagelessFramebuffer =
		graphicsBase::Base().ActiveRenderMode() == RenderMode::ImagelessFramebuffer;
	const bool useDynamicRendering =
		graphicsBase::Base().ActiveRenderMode() == RenderMode::DynamicRendering;
	if (useImagelessFramebuffer)
		(void)ImagelessRenderPassAndFramebuffer();
	else if (!useDynamicRendering)
		(void)LegacyRenderPassAndFramebuffers();
	CreateLayout();
	CreatePipeline();
	bufferMemory vertexBuffer_rectangle;
	if (vertexBuffer_rectangle.CreateDeviceLocal(vertices_rectangle, sizeof(vertices_rectangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return -1;
	bufferMemory indexBuffer_rectangle;
	if (indexBuffer_rectangle.CreateDeviceLocal(indices_rectangle, sizeof(indices_rectangle), VK_BUFFER_USAGE_INDEX_BUFFER_BIT))
		return -1;
	bufferMemory instanceBuffer_rectangles;
	if (instanceBuffer_rectangles.CreateDeviceLocal(instances_rectangle, sizeof(instances_rectangle), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return -1;

	constexpr uint32_t maxFramesInFlight = 1;
	std::array<bufferMemory, maxFramesInFlight> uniformBuffers;
	for (bufferMemory& uniformBuffer : uniformBuffers)
		if (uniformBuffer.CreateHostVisible(sizeof(UniformData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT))
			return -1;
	texture2d texture_scene;
	if (texture_scene.Create("textures/uv_orientation_test.png", VK_FORMAT_R8G8B8A8_SRGB))
		return -1;

	VkDescriptorPoolSize poolSizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, maxFramesInFlight },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxFramesInFlight }
	};
	descriptorPool descriptorPool_scene;
	if (descriptorPool_scene.Create(maxFramesInFlight, uint32_t(std::size(poolSizes)), poolSizes))
		return -1;
	std::array<VkDescriptorSet, maxFramesInFlight> descriptorSets_scene = {};
	if (descriptorPool_scene.Allocate(descriptorSetLayout_scene, maxFramesInFlight, descriptorSets_scene.data()))
		return -1;
	for (uint32_t i = 0; i < maxFramesInFlight; i++)
	{
		VkDescriptorBufferInfo bufferInfo = uniformBuffers[i].DescriptorInfo(sizeof(UniformData));
		VkDescriptorImageInfo imageInfo = texture_scene.DescriptorInfo();
		VkWriteDescriptorSet descriptorWrites[] =
		{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets_scene[i],
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = descriptorSets_scene[i],
				.dstBinding = 1,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo
			}
		};
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), uint32_t(std::size(descriptorWrites)), descriptorWrites, 0, nullptr);
	}

	// VkFence: 渲染提交完成后由 GPU 置位，CPU 在循环末尾等待并重置它。
	fence fence;
	// VkSemaphore: 获取交换链图像成功后置位，图形队列提交前等待它。
	semaphore semaphore_imageIsAvailable;
	// 呈现等待信号量按交换链图像索引分配；只有同一图像再次被获取时，
	// 上一次使用它的呈现操作才已完成，可以安全复用对应信号量。
	std::vector<semaphore> semaphores_renderingIsOver(graphicsBase::Base().SwapchainImageCount());

	// VkCommandBuffer: 每帧录制一次并提交到图形队列。
	commandBuffer commandBuffer;
	// VkCommandPool: 从图形队列族创建，负责分配 commandBuffer。
	commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	if (commandPool.AllocateBuffers(commandBuffer))
		return -1;

	// VkClearValue: 渲染通道开始时用于清空颜色附件的红色清屏值。
	VkClearValue clearColor = {};
	clearColor.color.float32[0] = 1.f;
	clearColor.color.float32[1] = 0.f;
	clearColor.color.float32[2] = 0.f;
	clearColor.color.float32[3] = 1.f;

	const double selfTestStartTime = glfwGetTime();
	bool selfTestResized = false;
	bool selfTestMinimized = false;
	bool selfTestRestored = false;
	while (!glfwWindowShouldClose(pWindow))
	{
		// 窗口最小化时暂停渲染循环，避免交换链图像获取和提交产生无意义错误。
		while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
		{
			glfwWaitEventsTimeout(0.1);
			if (selfTest && !selfTestRestored && glfwGetTime() - selfTestStartTime >= 2.5)
			{
				glfwRestoreWindow(pWindow);
				selfTestRestored = true;
				outStream << "[ SelfTest ] Window restored.\n";
			}
		}

		const double selfTestElapsed = glfwGetTime() - selfTestStartTime;
		if (selfTest && selfTestMinimized && !selfTestRestored && selfTestElapsed >= 2.5)
		{
			glfwRestoreWindow(pWindow);
			selfTestRestored = true;
			outStream << "[ SelfTest ] Window restored.\n";
		}
		if (selfTest && !selfTestResized && selfTestElapsed >= 0.5)
		{
			glfwSetWindowSize(pWindow, 960, 540);
			selfTestResized = true;
			outStream << "[ SelfTest ] Window resized to 960x540.\n";
		}
		if (selfTest && !selfTestMinimized && selfTestElapsed >= 1.5)
		{
			glfwIconifyWindow(pWindow);
			selfTestMinimized = true;
			outStream << "[ SelfTest ] Window minimized.\n";
			continue;
		}
		if (selfTest && selfTestElapsed >= 3.5)
		{
			outStream << "[ SelfTest ] Completed; requesting graceful shutdown.\n";
			glfwSetWindowShouldClose(pWindow, GLFW_TRUE);
			continue;
		}

		// 获取当前可写入的交换链图像索引，并在图像可用时置位 semaphore_imageIsAvailable。
		graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
		auto imageIndex = graphicsBase::Base().CurrentImageIndex();
		while (semaphores_renderingIsOver.size() < graphicsBase::Base().SwapchainImageCount())
			semaphores_renderingIsOver.emplace_back();
		VkSemaphore semaphore_renderingIsOver = semaphores_renderingIsOver[imageIndex];
		constexpr uint32_t frameIndex = 0;
		const float time = float(glfwGetTime());
		UniformData uniformData =
		{
			.model = glm::rotate(glm::mat4(1.0f), 0.15f * std::sin(time * 0.5f), glm::vec3(0.0f, 0.0f, 1.0f)),
			.view = glm::mat4(1.0f),
			.projection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, 0.0f, 1.0f)
		};
		uniformData.projection[1][1] *= -1.0f;
		if (uniformBuffers[frameIndex].Write(&uniformData, sizeof(uniformData)))
			return -1;

		// 开始录制当前帧命令。
		commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		// VkRenderPass + VkFramebuffer: 指定本帧渲染时使用的渲染通道和当前交换链图像对应的帧缓冲。
		if (useDynamicRendering)
		{
			// UNDEFINED 允许丢弃交换链图像旧内容；本帧会先 clear，因此无需跟踪先前的 PRESENT 布局。
			const VkImageSubresourceRange colorRange =
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, graphicsBase::Base().SwapchainCreateInfo().imageArrayLayers };
			imageOperation::CmdTransitionLayout(commandBuffer, graphicsBase::Base().SwapchainImage(imageIndex),
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, colorRange);

			VkRenderingAttachmentInfo colorAttachmentInfo =
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView = graphicsBase::Base().SwapchainImageView(imageIndex),
				.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.clearValue = clearColor
			};
			VkRenderingInfo renderingInfo =
			{
				.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
				.renderArea = { {}, windowSize },
				.layerCount = graphicsBase::Base().SwapchainCreateInfo().imageArrayLayers,
				.colorAttachmentCount = 1,
				.pColorAttachments = &colorAttachmentInfo,
				.pDepthAttachment = nullptr,
				.pStencilAttachment = nullptr
			};
			if (graphicsBase::Base().CmdBeginRendering(commandBuffer, renderingInfo))
				return -1;
		}
		else if (useImagelessFramebuffer)
		{
			const auto& rpwf = ImagelessRenderPassAndFramebuffer();
			rpwf.renderPass.CmdBeginImageless(commandBuffer, rpwf.framebuffer,
				graphicsBase::Base().SwapchainImageView(imageIndex), { {}, windowSize }, clearColor);
		}
		else
		{
			const auto& rpwf = LegacyRenderPassAndFramebuffers();
			rpwf.renderPass.CmdBegin(commandBuffer, rpwf.framebuffers[imageIndex], { {}, windowSize }, clearColor);
		}

		// VkPipeline: 绑定图形管线后，后续 draw 命令使用该管线状态执行。
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
		VkBuffer vertexBuffers[] = { vertexBuffer_rectangle, instanceBuffer_rectangles };
		VkDeviceSize vertexBufferOffsets[] = { 0, 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 2, vertexBuffers, vertexBufferOffsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer_rectangle, 0, VK_INDEX_TYPE_UINT16);
		const float pulse = 0.8f + 0.2f * (0.5f + 0.5f * std::sin(time * 2.0f));
		PushConstantData pushConstantData =
		{
			.color = {
				0.75f + 0.25f * (0.5f + 0.5f * std::sin(time)),
				0.75f + 0.25f * (0.5f + 0.5f * std::sin(time + 2.094f)),
				0.75f + 0.25f * (0.5f + 0.5f * std::sin(time + 4.189f)),
				1.0f },
			.scale = { pulse, pulse }
		};
		vkCmdPushConstants(commandBuffer, pipelineLayout_triangle, VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(pushConstantData), &pushConstantData);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_triangle,
			0, 1, &descriptorSets_scene[frameIndex], 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, uint32_t(std::size(indices_rectangle)), uint32_t(std::size(instances_rectangle)), 0, 0, 0);

		if (useDynamicRendering)
		{
			if (graphicsBase::Base().CmdEndRendering(commandBuffer))
				return -1;
			imageOperation::CmdTransitionLayout(commandBuffer, graphicsBase::Base().SwapchainImage(imageIndex),
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0,
				{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, graphicsBase::Base().SwapchainCreateInfo().imageArrayLayers });
		}
		else if (useImagelessFramebuffer)
			ImagelessRenderPassAndFramebuffer().renderPass.CmdEnd(commandBuffer);
		else
			LegacyRenderPassAndFramebuffers().renderPass.CmdEnd(commandBuffer);
		commandBuffer.End();

		// 提交命令缓冲区：等待图像可用信号量，完成后置位渲染结束信号量和 fence。
		graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
		// 呈现当前交换链图像：等待渲染结束信号量。
		graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

		glfwPollEvents();
		TitleFps();

		// 等待本帧提交完成并重置 fence，下一帧才能复用同一个命令缓冲区和同步对象。
		fence.WaitAndReset();
	}

	// fence 只覆盖图形提交，不覆盖 WSI 呈现；在局部信号量析构前等待呈现队列。
	if (VkResult result = vkQueueWaitIdle(graphicsBase::Base().Queue_Presentation()))
		return int(result);

	return 0;
}

int main(int argc, char* argv[])
{
	bool selfTest = false;
	RenderMode requestedRenderMode = RenderMode::DynamicRendering;
	RenderExample renderExample = RenderExample::Forward;
	HdrPreference hdrPreference = HdrPreference::Auto;
	for (int i = 1; i < argc; i++)
	{
		const std::string_view argument = argv[i];
		if (ParseRenderExample(argument, renderExample) || ParseHdrPreference(argument, hdrPreference))
			continue;
		if (argument == "--self-test")
			selfTest = true;
		else if (argument == "--mode=legacy")
			requestedRenderMode = RenderMode::LegacyRenderPass;
		else if (argument == "--mode=imageless")
			requestedRenderMode = RenderMode::ImagelessFramebuffer;
		else if (argument == "--mode=dynamic")
			requestedRenderMode = RenderMode::DynamicRendering;
	}
	// 离屏、深度、延迟、Alpha 和色彩输出示例使用传统 Render Pass；Forward 模式保留三路径切换。
	if (renderExample != RenderExample::Forward)
		requestedRenderMode = RenderMode::LegacyRenderPass;
	const bool requestHdrFormats = hdrPreference == HdrPreference::RequestHDR ||
		(renderExample == RenderExample::HDR && hdrPreference == HdrPreference::Auto);
	if (requestHdrFormats)
	{
		const char* colorSpaceExtension[] = { VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME };
		if (!graphicsBase::Base().CheckInstanceExtensions(colorSpaceExtension) && colorSpaceExtension[0])
			graphicsBase::Base().AddInstanceExtension(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
		else
			outStream << "[ ColorOutput ] VK_EXT_swapchain_colorspace is unavailable; HDR will fall back to SDR.\n";
	}
	graphicsBase::Base().RequestRenderMode(requestedRenderMode);

	if (!InitializeWindow({ 1280, 720 }))
	{
		TerminateWindow();
		return -1;
	}

	const int result = renderExample == RenderExample::Forward
		? Run(selfTest)
		: RunRenderExample(renderExample, hdrPreference);
	TerminateWindow();
	return result;
}
