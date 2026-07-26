#include "GLfwGeneral.hpp"
#include "MyVulkan.h"
#include <cstddef>

using namespace vulkan;
using namespace easyVulkan;

void TitleFps();

struct Vertex
{
	glm::vec2 position;
	glm::vec4 color;
};

const Vertex vertices_triangle[] =
{
	{ {  0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
	{ { -0.5f,  0.5f }, { 0.0f, 1.0f, 0.0f, 1.0f } },
	{ {  0.5f,  0.5f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
};

// VkPipelineLayout: 三角形管线使用的布局，本节不包含描述符集和 push constant。
pipelineLayout pipelineLayout_triangle;
// VkPipeline: 三角形绘制使用的图形管线。
pipeline pipeline_triangle;

const auto& RenderPassAndFramebuffers()
{
	static const auto& rpwf = easyVulkan::CreateRpwf_Screen();
	return rpwf;
}

void CreateLayout()
{
	// VkPipelineLayoutCreateInfo: 本节 shader 不访问外部资源，因此创建空管线布局。
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
	static bool callbackAdded = false;
	if (!callbackAdded)
	{
		graphicsBase::Base().AddCallback_DestroyDevice([] { pipelineLayout_triangle.Destroy(); });
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
		pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
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

int Run()
{
	const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
	CreateLayout();
	CreatePipeline();
	vertexBuffer vertexBuffer_triangle;
	if (vertexBuffer_triangle.Create(vertices_triangle, sizeof(vertices_triangle)))
		return -1;

	// VkFence: 渲染提交完成后由 GPU 置位，CPU 在循环末尾等待并重置它。
	fence fence;
	// VkSemaphore: 获取交换链图像成功后置位，图形队列提交前等待它。
	semaphore semaphore_imageIsAvailable;
	// VkSemaphore: 命令缓冲区执行完成后置位，呈现图像前等待它。
	semaphore semaphore_renderingIsOver;

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

	while (!glfwWindowShouldClose(pWindow))
	{
		// 窗口最小化时暂停渲染循环，避免交换链图像获取和提交产生无意义错误。
		while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
			glfwWaitEvents();

		// 获取当前可写入的交换链图像索引，并在图像可用时置位 semaphore_imageIsAvailable。
		graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
		auto imageIndex = graphicsBase::Base().CurrentImageIndex();

		// 开始录制当前帧命令。
		commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		// VkRenderPass + VkFramebuffer: 指定本帧渲染时使用的渲染通道和当前交换链图像对应的帧缓冲。
		renderPass.CmdBegin(commandBuffer, framebuffers[imageIndex], { {}, windowSize }, clearColor);

		// VkPipeline: 绑定图形管线后，后续 draw 命令使用该管线状态执行。
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
		VkDeviceSize vertexBufferOffset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffer_triangle.Address(), &vertexBufferOffset);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		renderPass.CmdEnd(commandBuffer);
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

	return 0;
}

int main()
{
	if (!InitializeWindow({ 1280, 720 }))
	{
		TerminateWindow();
		return -1;
	}

	const int result = Run();
	TerminateWindow();
	return result;
}
