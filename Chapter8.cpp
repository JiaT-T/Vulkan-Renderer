#include "Chapter8.h"
#include "MyVulkan.h"
#include <GLFW/glfw3.h>
#include <array>
#include <cmath>

extern GLFWwindow* pWindow;
void TitleFps();

using namespace vulkan;
using namespace easyVulkan;

namespace
{
constexpr VkFormat offscreenColorFormat = VK_FORMAT_R8G8B8A8_UNORM;

bool FormatSupports(VkFormat format, VkFormatFeatureFlags requiredFeatures)
{
	VkFormatProperties properties = {};
	vkGetPhysicalDeviceFormatProperties(graphicsBase::Base().PhysicalDevice(), format, &properties);
	return (properties.optimalTilingFeatures & requiredFeatures) == requiredFeatures;
}

VkResult CreateFullscreenSampler(VkSampler& sampler)
{
	VkSamplerCreateInfo createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.maxLod = 0.0f,
		.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
	};
	return vkCreateSampler(graphicsBase::Base().Device(), &createInfo, nullptr, &sampler);
}

VkResult CreateNoVertexPipeline(shaderModule& vertexShader, shaderModule& fragmentShader,
	VkPipelineLayout layout, VkRenderPass renderPass, uint32_t subpass, pipeline& output,
	VkExtent2D extent, const std::vector<VkPipelineColorBlendAttachmentState>& blendAttachments)
{
	VkPipelineShaderStageCreateInfo stages[] =
	{
		vertexShader.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
		fragmentShader.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	graphicsPipelineCreateInfoPack pack;
	pack.createInfo.layout = layout;
	pack.createInfo.renderPass = renderPass;
	pack.createInfo.subpass = subpass;
	pack.createInfo.stageCount = uint32_t(std::size(stages));
	pack.createInfo.pStages = stages;
	pack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pack.viewports.push_back({ 0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f, 1.0f });
	pack.scissors.push_back({ {}, extent });
	pack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pack.colorBlendAttachmentStates = blendAttachments;
	pack.UpdateAllArrays();
	return output.Create(pack);
}

class Chapter8Renderer;
Chapter8Renderer* activeRenderer = nullptr;

class Chapter8Renderer
{
public:
	Chapter8Renderer(RenderExample example, HdrPreference hdrPreference)
		: example(example), hdrPreference(hdrPreference)
	{
		activeRenderer = this;
	}

	~Chapter8Renderer()
	{
		DestroySwapchainResources();
		DestroyDeviceResources();
		activeRenderer = nullptr;
	}

	VkResult Initialize()
	{
		VkResult result = CreateDeviceResources();
		if (result)
			return result;
		RegisterSwapchainCallbacks();
		return CreateSwapchainResources();
	}

	int Run()
	{
		if (example != RenderExample::Offscreen)
		{
			outStream << "[ Chapter8 ] ERROR\nThe requested Chapter 8 example has not been initialized.\n";
			return -1;
		}

		fence frameFence;
		semaphore imageAvailable;
		std::vector<semaphore> renderFinished(graphicsBase::Base().SwapchainImageCount());
		commandBuffer commandBuffer;
		commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(),
			VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
		if (commandPool.AllocateBuffers(commandBuffer))
			return -1;

		while (!glfwWindowShouldClose(pWindow))
		{
			while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
				glfwWaitEvents();

			if (swapchainRecreationFailed)
				return -1;
			if (VkResult result = graphicsBase::Base().SwapImage(imageAvailable))
				return int(result);
			const uint32_t imageIndex = graphicsBase::Base().CurrentImageIndex();
			while (renderFinished.size() < graphicsBase::Base().SwapchainImageCount())
				renderFinished.emplace_back();

			if (VkResult result = commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
				return int(result);
			RecordOffscreenFrame(commandBuffer, imageIndex);
			if (VkResult result = commandBuffer.End())
				return int(result);

			VkSemaphore finished = renderFinished[imageIndex];
			if (VkResult result = graphicsBase::Base().SubmitCommandBuffer_Graphics(
				commandBuffer, imageAvailable, finished, frameFence))
				return int(result);
			if (VkResult result = graphicsBase::Base().PresentImage(finished))
				return int(result);

			glfwPollEvents();
			TitleFps();
			if (VkResult result = frameFence.WaitAndReset())
				return int(result);
		}

		return int(vkQueueWaitIdle(graphicsBase::Base().Queue_Presentation()));
	}

	VkResult CreateSwapchainResources()
	{
		DestroySwapchainResources();
		swapchainRecreationFailed = false;
		VkResult result = CreateOffscreenImage();
		if (!result)
			result = CreateOffscreenRenderPass();
		if (!result)
			result = CreateScreenRenderPass();
		if (!result)
			result = CreateFramebuffers();
		if (!result)
			result = CreateOffscreenDescriptor();
		if (!result)
			result = CreateOffscreenPipelines();
		if (result)
		{
			swapchainRecreationFailed = true;
			DestroySwapchainResources();
		}
		return result;
	}

	void DestroySwapchainResources()
	{
		screenFramebuffers.clear();
		offscreenFramebuffer.Destroy();
		offscreenPipeline.Destroy();
		fullscreenPipeline.Destroy();
		offscreenRenderPass.Destroy();
		screenRenderPass.Destroy();
		offscreenDescriptorPool.Destroy();
		offscreenColor.Destroy();
		offscreenDescriptorSet = VK_NULL_HANDLE;
	}

private:
	RenderExample example = RenderExample::Forward;
	HdrPreference hdrPreference = HdrPreference::Auto;
	bool callbacksRegistered = false;
	bool swapchainRecreationFailed = false;

	imageMemory offscreenColor;
	VkSampler offscreenSampler = VK_NULL_HANDLE;
	renderPass offscreenRenderPass;
	renderPass screenRenderPass;
	framebuffer offscreenFramebuffer;
	std::vector<framebuffer> screenFramebuffers;
	pipeline offscreenPipeline;
	pipeline fullscreenPipeline;
	pipelineLayout offscreenPipelineLayout;
	pipelineLayout fullscreenPipelineLayout;
	descriptorSetLayout offscreenDescriptorSetLayout;
	descriptorPool offscreenDescriptorPool;
	VkDescriptorSet offscreenDescriptorSet = VK_NULL_HANDLE;
	shaderModule offscreenVertexShader;
	shaderModule offscreenFragmentShader;
	shaderModule fullscreenVertexShader;
	shaderModule fullscreenFragmentShader;

	void RegisterSwapchainCallbacks();

	VkResult CreateDeviceResources()
	{
		if (example != RenderExample::Offscreen)
			return VK_SUCCESS;

		VkPushConstantRange pushRange =
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(glm::vec4)
		};
		VkPipelineLayoutCreateInfo offscreenLayoutCi =
		{
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushRange
		};
		VkResult result = offscreenPipelineLayout.Create(offscreenLayoutCi);
		if (result)
			return result;

		VkDescriptorSetLayoutBinding sampledColorBinding =
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo setLayoutCi =
		{
			.bindingCount = 1,
			.pBindings = &sampledColorBinding
		};
		result = offscreenDescriptorSetLayout.Create(setLayoutCi);
		if (result)
			return result;
		VkPipelineLayoutCreateInfo fullscreenLayoutCi =
		{
			.setLayoutCount = 1,
			.pSetLayouts = offscreenDescriptorSetLayout.Address()
		};
		result = fullscreenPipelineLayout.Create(fullscreenLayoutCi);
		if (result)
			return result;
		result = CreateFullscreenSampler(offscreenSampler);
		if (result)
			return result;
		if ((result = offscreenVertexShader.Create("shader/Offscreen.vert.spv")) ||
			(result = offscreenFragmentShader.Create("shader/Offscreen.frag.spv")) ||
			(result = fullscreenVertexShader.Create("shader/Fullscreen.vert.spv")) ||
			(result = fullscreenFragmentShader.Create("shader/Fullscreen.frag.spv")))
			return result;
		return VK_SUCCESS;
	}

	void DestroyDeviceResources()
	{
		offscreenVertexShader.Destroy();
		offscreenFragmentShader.Destroy();
		fullscreenVertexShader.Destroy();
		fullscreenFragmentShader.Destroy();
		if (offscreenSampler)
		{
			vkDestroySampler(graphicsBase::Base().Device(), offscreenSampler, nullptr);
			offscreenSampler = VK_NULL_HANDLE;
		}
		fullscreenPipelineLayout.Destroy();
		offscreenPipelineLayout.Destroy();
		offscreenDescriptorSetLayout.Destroy();
	}

	VkResult CreateOffscreenImage()
	{
		const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		if (!FormatSupports(offscreenColorFormat, required))
		{
			outStream << "[ Ch8-1 ] ERROR\nR8G8B8A8_UNORM does not support the required offscreen usages.\n";
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		}
		const VkExtent3D extent = { windowSize.width, windowSize.height, 1 };
		VkResult result = offscreenColor.Create(extent, 1, offscreenColorFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result)
			result = offscreenColor.CreateView(VK_IMAGE_ASPECT_COLOR_BIT);
		return result;
	}

	VkResult CreateOffscreenRenderPass()
	{
		VkAttachmentDescription colorAttachment =
		{
			.format = offscreenColorFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass =
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorReference
		};
		VkSubpassDependency dependencies[] =
		{
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass = 0,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			}
		};
		VkRenderPassCreateInfo createInfo =
		{
			.attachmentCount = 1,
			.pAttachments = &colorAttachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(std::size(dependencies)),
			.pDependencies = dependencies
		};
		return offscreenRenderPass.Create(createInfo);
	}

	VkResult CreateScreenRenderPass()
	{
		VkAttachmentDescription colorAttachment =
		{
			.format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
		};
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass =
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorReference
		};
		VkSubpassDependency dependency =
		{
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
		};
		VkRenderPassCreateInfo createInfo =
		{
			.attachmentCount = 1,
			.pAttachments = &colorAttachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dependency
		};
		return screenRenderPass.Create(createInfo);
	}

	VkResult CreateFramebuffers()
	{
		VkImageView offscreenAttachment = offscreenColor.View();
		VkFramebufferCreateInfo offscreenCi =
		{
			.renderPass = offscreenRenderPass,
			.attachmentCount = 1,
			.pAttachments = &offscreenAttachment,
			.width = windowSize.width,
			.height = windowSize.height,
			.layers = 1
		};
		VkResult result = offscreenFramebuffer.Create(offscreenCi);
		if (result)
			return result;

		screenFramebuffers.resize(graphicsBase::Base().SwapchainImageCount());
		for (uint32_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++)
		{
			VkImageView screenAttachment = graphicsBase::Base().SwapchainImageView(i);
			VkFramebufferCreateInfo screenCi =
			{
				.renderPass = screenRenderPass,
				.attachmentCount = 1,
				.pAttachments = &screenAttachment,
				.width = windowSize.width,
				.height = windowSize.height,
				.layers = 1
			};
			if ((result = screenFramebuffers[i].Create(screenCi)))
				return result;
		}
		return VK_SUCCESS;
	}

	VkResult CreateOffscreenDescriptor()
	{
		VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
		VkResult result = offscreenDescriptorPool.Create(1, 1, &poolSize);
		if (result)
			return result;
		result = offscreenDescriptorPool.Allocate(offscreenDescriptorSetLayout, 1, &offscreenDescriptorSet);
		if (result)
			return result;
		VkDescriptorImageInfo imageInfo =
		{
			.sampler = offscreenSampler,
			.imageView = offscreenColor.View(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet write =
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = offscreenDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo
		};
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), 1, &write, 0, nullptr);
		return VK_SUCCESS;
	}

	VkResult CreateOffscreenPipelines()
	{
		VkPipelineColorBlendAttachmentState opaqueBlend =
		{
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		VkResult result = CreateNoVertexPipeline(offscreenVertexShader, offscreenFragmentShader,
			offscreenPipelineLayout, offscreenRenderPass, 0, offscreenPipeline, windowSize, { opaqueBlend });
		if (!result)
			result = CreateNoVertexPipeline(fullscreenVertexShader, fullscreenFragmentShader,
				fullscreenPipelineLayout, screenRenderPass, 0, fullscreenPipeline, windowSize, { opaqueBlend });
		return result;
	}

	void RecordOffscreenFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue offscreenClear = {};
		offscreenClear.color = { { 0.025f, 0.035f, 0.075f, 1.0f } };
		offscreenRenderPass.CmdBegin(commandBuffer, offscreenFramebuffer, { {}, windowSize }, offscreenClear);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, offscreenPipeline);
		const glm::vec4 scenePush = { float(glfwGetTime()), 0.0f, 0.0f, 0.0f };
		vkCmdPushConstants(commandBuffer, offscreenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(scenePush), &scenePush);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		offscreenRenderPass.CmdEnd(commandBuffer);

		VkClearValue screenClear = {};
		screenClear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		screenRenderPass.CmdBegin(commandBuffer, screenFramebuffers[imageIndex], { {}, windowSize }, screenClear);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipelineLayout,
			0, 1, &offscreenDescriptorSet, 0, nullptr);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		screenRenderPass.CmdEnd(commandBuffer);
	}
};

void CreateChapter8SwapchainResources()
{
	if (activeRenderer && activeRenderer->CreateSwapchainResources())
		outStream << "[ Chapter8 ] ERROR\nFailed to recreate swapchain-dependent Chapter 8 resources.\n";
}

void DestroyChapter8SwapchainResources()
{
	if (activeRenderer)
		activeRenderer->DestroySwapchainResources();
}

void Chapter8Renderer::RegisterSwapchainCallbacks()
{
	if (callbacksRegistered)
		return;
	graphicsBase::Base().AddCallback_CreateSwapchain(CreateChapter8SwapchainResources);
	graphicsBase::Base().AddCallback_DestroySwapchain(DestroyChapter8SwapchainResources);
	callbacksRegistered = true;
}
}

bool ParseRenderExample(std::string_view argument, RenderExample& example)
{
	if (!argument.starts_with("--example="))
		return false;
	const std::string_view value = argument.substr(10);
	if (value == "forward") example = RenderExample::Forward;
	else if (value == "offscreen") example = RenderExample::Offscreen;
	else if (value == "depth") example = RenderExample::DepthTest;
	else if (value == "depth-raw") example = RenderExample::DepthRaw;
	else if (value == "depth-linear") example = RenderExample::DepthLinear;
	else if (value == "deferred") example = RenderExample::Deferred;
	else if (value == "gbuffer-albedo") example = RenderExample::DeferredAlbedo;
	else if (value == "gbuffer-normal") example = RenderExample::DeferredNormal;
	else if (value == "gbuffer-position") example = RenderExample::DeferredPosition;
	else if (value == "alpha") example = RenderExample::AlphaComparison;
	else if (value == "srgb") example = RenderExample::SRGBTest;
	else if (value == "sdr") example = RenderExample::SDRToneMapping;
	else if (value == "hdr") example = RenderExample::HDR;
	else return false;
	return true;
}

bool ParseHdrPreference(std::string_view argument, HdrPreference& preference)
{
	if (!argument.starts_with("--hdr="))
		return false;
	const std::string_view value = argument.substr(6);
	if (value == "auto") preference = HdrPreference::Auto;
	else if (value == "sdr") preference = HdrPreference::ForceSDR;
	else if (value == "request") preference = HdrPreference::RequestHDR;
	else return false;
	return true;
}

const char* RenderExampleName(RenderExample example)
{
	switch (example)
	{
	case RenderExample::Offscreen: return "Offscreen";
	case RenderExample::DepthTest: return "DepthTest";
	case RenderExample::DepthRaw: return "DepthRaw";
	case RenderExample::DepthLinear: return "DepthLinear";
	case RenderExample::Deferred: return "Deferred";
	case RenderExample::DeferredAlbedo: return "DeferredAlbedo";
	case RenderExample::DeferredNormal: return "DeferredNormal";
	case RenderExample::DeferredPosition: return "DeferredPosition";
	case RenderExample::AlphaComparison: return "AlphaComparison";
	case RenderExample::SRGBTest: return "SRGBTest";
	case RenderExample::SDRToneMapping: return "SDRToneMapping";
	case RenderExample::HDR: return "HDR";
	default: return "Forward";
	}
}

int RunChapter8Example(RenderExample example, HdrPreference hdrPreference)
{
	outStream << std::format("[ Chapter8 ] Example: {}\n", RenderExampleName(example));
	Chapter8Renderer renderer(example, hdrPreference);
	if (VkResult result = renderer.Initialize())
		return int(result);
	return renderer.Run();
}
