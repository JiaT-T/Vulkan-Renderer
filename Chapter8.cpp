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
constexpr float depthNearPlane = 0.1f;
constexpr float depthFarPlane = 50.0f;

bool FormatSupports(VkFormat format, VkFormatFeatureFlags requiredFeatures);

struct CubeVertex
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
};

struct alignas(16) DepthSceneUniform
{
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 projection;
	alignas(16) glm::mat4 models[3];
	alignas(16) glm::vec4 nearFarMode;
};

const CubeVertex cubeVertices[] =
{
	// +Z
	{{-0.5f,-0.5f, 0.5f},{0,0,1},{1.0f,0.25f,0.2f}}, {{ 0.5f,-0.5f, 0.5f},{0,0,1},{1.0f,0.25f,0.2f}},
	{{ 0.5f, 0.5f, 0.5f},{0,0,1},{1.0f,0.25f,0.2f}}, {{-0.5f, 0.5f, 0.5f},{0,0,1},{1.0f,0.25f,0.2f}},
	// -Z
	{{ 0.5f,-0.5f,-0.5f},{0,0,-1},{0.2f,0.75f,1.0f}}, {{-0.5f,-0.5f,-0.5f},{0,0,-1},{0.2f,0.75f,1.0f}},
	{{-0.5f, 0.5f,-0.5f},{0,0,-1},{0.2f,0.75f,1.0f}}, {{ 0.5f, 0.5f,-0.5f},{0,0,-1},{0.2f,0.75f,1.0f}},
	// +X
	{{ 0.5f,-0.5f, 0.5f},{1,0,0},{0.3f,1.0f,0.35f}}, {{ 0.5f,-0.5f,-0.5f},{1,0,0},{0.3f,1.0f,0.35f}},
	{{ 0.5f, 0.5f,-0.5f},{1,0,0},{0.3f,1.0f,0.35f}}, {{ 0.5f, 0.5f, 0.5f},{1,0,0},{0.3f,1.0f,0.35f}},
	// -X
	{{-0.5f,-0.5f,-0.5f},{-1,0,0},{1.0f,0.75f,0.18f}}, {{-0.5f,-0.5f, 0.5f},{-1,0,0},{1.0f,0.75f,0.18f}},
	{{-0.5f, 0.5f, 0.5f},{-1,0,0},{1.0f,0.75f,0.18f}}, {{-0.5f, 0.5f,-0.5f},{-1,0,0},{1.0f,0.75f,0.18f}},
	// +Y
	{{-0.5f, 0.5f, 0.5f},{0,1,0},{0.75f,0.25f,1.0f}}, {{ 0.5f, 0.5f, 0.5f},{0,1,0},{0.75f,0.25f,1.0f}},
	{{ 0.5f, 0.5f,-0.5f},{0,1,0},{0.75f,0.25f,1.0f}}, {{-0.5f, 0.5f,-0.5f},{0,1,0},{0.75f,0.25f,1.0f}},
	// -Y
	{{-0.5f,-0.5f,-0.5f},{0,-1,0},{0.2f,0.95f,0.85f}}, {{ 0.5f,-0.5f,-0.5f},{0,-1,0},{0.2f,0.95f,0.85f}},
	{{ 0.5f,-0.5f, 0.5f},{0,-1,0},{0.2f,0.95f,0.85f}}, {{-0.5f,-0.5f, 0.5f},{0,-1,0},{0.2f,0.95f,0.85f}}
};

const uint16_t cubeIndices[] =
{
	0,1,2, 2,3,0, 4,5,6, 6,7,4, 8,9,10, 10,11,8,
	12,13,14, 14,15,12, 16,17,18, 18,19,16, 20,21,22, 22,23,20
};

bool IsDepthExample(RenderExample example)
{
	return example == RenderExample::DepthTest || example == RenderExample::DepthRaw ||
		example == RenderExample::DepthLinear;
}

bool IsDeferredExample(RenderExample example)
{
	return example == RenderExample::Deferred || example == RenderExample::DeferredAlbedo ||
		example == RenderExample::DeferredNormal || example == RenderExample::DeferredPosition;
}

bool IsAlphaExample(RenderExample example)
{
	return example == RenderExample::AlphaComparison;
}

bool IsColorOutputExample(RenderExample example)
{
	return example == RenderExample::SRGBTest || example == RenderExample::SDRToneMapping ||
		example == RenderExample::HDR;
}

VkFormat FindDepthFormat()
{
	const VkFormat candidates[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM };
	const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT |
		VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
	for (VkFormat format : candidates)
		if (FormatSupports(format, required))
			return format;
	return VK_FORMAT_UNDEFINED;
}

void BeginRenderPass(VkCommandBuffer commandBuffer, VkRenderPass renderPass, VkFramebuffer framebuffer,
	VkExtent2D extent, std::span<const VkClearValue> clearValues)
{
	VkRenderPassBeginInfo beginInfo =
	{
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = renderPass,
		.framebuffer = framebuffer,
		.renderArea = { {}, extent },
		.clearValueCount = uint32_t(clearValues.size()),
		.pClearValues = clearValues.data()
	};
	vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

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

VkResult CreateDepthScenePipeline(shaderModule& vertexShader, shaderModule& fragmentShader,
	VkPipelineLayout layout, VkRenderPass renderPass, pipeline& output, VkExtent2D extent,
	uint32_t colorAttachmentCount = 1)
{
	VkPipelineShaderStageCreateInfo stages[] =
	{
		vertexShader.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
		fragmentShader.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
	};
	graphicsPipelineCreateInfoPack pack;
	pack.createInfo.layout = layout;
	pack.createInfo.renderPass = renderPass;
	pack.createInfo.stageCount = uint32_t(std::size(stages));
	pack.createInfo.pStages = stages;
	pack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pack.vertexInputBindings.push_back({ 0, sizeof(CubeVertex), VK_VERTEX_INPUT_RATE_VERTEX });
	pack.vertexInputAttributes.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, position) });
	pack.vertexInputAttributes.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, normal) });
	pack.vertexInputAttributes.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(CubeVertex, color) });
	pack.viewports.push_back({ 0.0f, 0.0f, float(extent.width), float(extent.height), 0.0f, 1.0f });
	pack.scissors.push_back({ {}, extent });
	pack.rasterizationStateCi.cullMode = VK_CULL_MODE_BACK_BIT;
	pack.rasterizationStateCi.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	pack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pack.depthStencilStateCi.depthTestEnable = VK_TRUE;
	pack.depthStencilStateCi.depthWriteEnable = VK_TRUE;
	pack.depthStencilStateCi.depthCompareOp = VK_COMPARE_OP_LESS;
	pack.depthStencilStateCi.maxDepthBounds = 1.0f;
	for (uint32_t i = 0; i < colorAttachmentCount; i++)
		pack.colorBlendAttachmentStates.push_back(
			{ .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT });
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
		VkResult result = IsColorOutputExample(example) ? ConfigureOutputSurface() : VK_SUCCESS;
		if (!result)
			result = CreateDeviceResources();
		if (result)
			return result;
		RegisterSwapchainCallbacks();
		return CreateSwapchainResources();
	}

	int Run()
	{
		if (example != RenderExample::Offscreen && !IsDepthExample(example) &&
			!IsDeferredExample(example) && !IsAlphaExample(example) && !IsColorOutputExample(example))
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
			if (IsDepthExample(example) || IsDeferredExample(example))
			{
				if (VkResult result = UpdateDepthUniform())
					return int(result);
			}

			if (VkResult result = commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
				return int(result);
			if (example == RenderExample::Offscreen)
				RecordOffscreenFrame(commandBuffer, imageIndex);
			else if (IsDeferredExample(example))
				RecordDeferredFrame(commandBuffer, imageIndex);
			else if (IsAlphaExample(example))
				RecordAlphaFrame(commandBuffer, imageIndex);
			else if (IsColorOutputExample(example))
				RecordColorOutputFrame(commandBuffer, imageIndex);
			else
				RecordDepthFrame(commandBuffer, imageIndex);
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
		VkResult result = VK_SUCCESS;
		if (IsColorOutputExample(example))
		{
			result = CreateScreenRenderPass();
			if (!result) result = CreateScreenFramebuffersOnly();
			if (example != RenderExample::SRGBTest)
			{
				if (!result) result = CreateHdrTarget();
				if (!result) result = CreateHdrRenderPassAndFramebuffer();
				if (!result) result = CreateColorOutputDescriptor();
			}
			if (!result) result = CreateColorOutputPipelines();
		}
		else if (IsAlphaExample(example))
		{
			result = CreateScreenRenderPass();
			if (!result) result = CreateScreenFramebuffersOnly();
			if (!result) result = CreateAlphaDescriptors();
			if (!result) result = CreateAlphaPipelines();
		}
		else if (IsDeferredExample(example))
		{
			result = CreateGBufferImages();
			if (!result) result = CreateDeferredRenderPass();
			if (!result) result = CreateDeferredFramebuffers();
			if (!result) result = CreateDeferredDescriptors();
			if (!result) result = CreateDeferredPipelines();
		}
		else
		{
			result = CreateOffscreenImage();
			if (!result && IsDepthExample(example)) result = CreateDepthImage();
			if (!result && IsDepthExample(example)) result = CreateDepthRenderPass();
			else if (!result) result = CreateOffscreenRenderPass();
			if (!result) result = CreateScreenRenderPass();
			if (!result) result = CreateFramebuffers();
			if (!result) result = CreateDescriptors();
			if (!result) result = IsDepthExample(example) ? CreateDepthPipelines() : CreateOffscreenPipelines();
		}
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
		deferredFramebuffers.clear();
		offscreenFramebuffer.Destroy();
		depthFramebuffer.Destroy();
		offscreenPipeline.Destroy();
		fullscreenPipeline.Destroy();
		depthScenePipeline.Destroy();
		depthVisualizePipeline.Destroy();
		deferredGeometryPipeline.Destroy();
		deferredCompositionPipeline.Destroy();
		straightAlphaPipeline.Destroy();
		incorrectPremultipliedPipeline.Destroy();
		premultipliedAlphaPipeline.Destroy();
		colorSpaceTestPipeline.Destroy();
		hdrScenePipeline.Destroy();
		toneMappingPipeline.Destroy();
		offscreenRenderPass.Destroy();
		depthRenderPass.Destroy();
		deferredRenderPass.Destroy();
		hdrFramebuffer.Destroy();
		hdrRenderPass.Destroy();
		screenRenderPass.Destroy();
		offscreenDescriptorPool.Destroy();
		deferredDescriptorPool.Destroy();
		alphaDescriptorPool.Destroy();
		colorOutputDescriptorPool.Destroy();
		hdrColor.Destroy();
		gbufferAlbedo.Destroy();
		gbufferNormal.Destroy();
		gbufferPosition.Destroy();
		depthImage.Destroy();
		offscreenColor.Destroy();
		offscreenDescriptorSet = VK_NULL_HANDLE;
		depthDescriptorSet = VK_NULL_HANDLE;
		depthSceneDescriptorSet = VK_NULL_HANDLE;
		deferredInputDescriptorSet = VK_NULL_HANDLE;
		straightAlphaDescriptorSet = VK_NULL_HANDLE;
		premultipliedAlphaDescriptorSet = VK_NULL_HANDLE;
		colorOutputDescriptorSet = VK_NULL_HANDLE;
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
	VkDescriptorSet depthDescriptorSet = VK_NULL_HANDLE;
	shaderModule offscreenVertexShader;
	shaderModule offscreenFragmentShader;
	shaderModule fullscreenVertexShader;
	shaderModule fullscreenFragmentShader;

	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	imageMemory depthImage;
	renderPass depthRenderPass;
	framebuffer depthFramebuffer;
	pipeline depthScenePipeline;
	pipeline depthVisualizePipeline;
	pipelineLayout depthScenePipelineLayout;
	descriptorSetLayout depthSceneDescriptorSetLayout;
	VkDescriptorSet depthSceneDescriptorSet = VK_NULL_HANDLE;
	bufferMemory cubeVertexBuffer;
	bufferMemory cubeIndexBuffer;
	bufferMemory depthUniformBuffer;
	shaderModule depthSceneVertexShader;
	shaderModule depthSceneFragmentShader;
	shaderModule depthVisualizeFragmentShader;

	imageMemory gbufferAlbedo;
	imageMemory gbufferNormal;
	imageMemory gbufferPosition;
	renderPass deferredRenderPass;
	std::vector<framebuffer> deferredFramebuffers;
	pipeline deferredGeometryPipeline;
	pipeline deferredCompositionPipeline;
	pipelineLayout deferredCompositionPipelineLayout;
	descriptorSetLayout deferredInputDescriptorSetLayout;
	descriptorPool deferredDescriptorPool;
	VkDescriptorSet deferredInputDescriptorSet = VK_NULL_HANDLE;
	shaderModule deferredGeometryVertexShader;
	shaderModule deferredGeometryFragmentShader;
	shaderModule deferredCompositionFragmentShader;

	texture2d straightAlphaTexture;
	texture2d premultipliedAlphaTexture;
	descriptorSetLayout alphaDescriptorSetLayout;
	descriptorPool alphaDescriptorPool;
	VkDescriptorSet straightAlphaDescriptorSet = VK_NULL_HANDLE;
	VkDescriptorSet premultipliedAlphaDescriptorSet = VK_NULL_HANDLE;
	pipelineLayout alphaPipelineLayout;
	pipeline straightAlphaPipeline;
	pipeline incorrectPremultipliedPipeline;
	pipeline premultipliedAlphaPipeline;
	shaderModule alphaVertexShader;
	shaderModule alphaFragmentShader;

	bool outputIsSrgbAttachment = false;
	bool hdrOutputActive = false;
	uint32_t hdrOutputEncoding = 0; // 0 SDR, 2 scRGB, 3 HDR10 PQ
	imageMemory hdrColor;
	VkSampler hdrSampler = VK_NULL_HANDLE;
	renderPass hdrRenderPass;
	framebuffer hdrFramebuffer;
	descriptorSetLayout colorOutputDescriptorSetLayout;
	descriptorPool colorOutputDescriptorPool;
	VkDescriptorSet colorOutputDescriptorSet = VK_NULL_HANDLE;
	pipelineLayout colorOutputPipelineLayout;
	pipeline colorSpaceTestPipeline;
	pipeline hdrScenePipeline;
	pipeline toneMappingPipeline;
	shaderModule colorSpaceTestFragmentShader;
	shaderModule hdrSceneFragmentShader;
	shaderModule toneMappingFragmentShader;

	void RegisterSwapchainCallbacks();

	VkResult ConfigureOutputSurface()
	{
		const VkSwapchainCreateInfoKHR& current = graphicsBase::Base().SwapchainCreateInfo();
		VkSurfaceFormatKHR selected = { current.imageFormat, current.imageColorSpace };
		VkSurfaceFormatKHR sdrCandidate = selected;
		VkSurfaceFormatKHR hdrCandidate = {};
		uint32_t hdrCandidateEncoding = 0;

		for (uint32_t i = 0; i < graphicsBase::Base().AvailableSurfaceFormatCount(); i++)
		{
			const VkSurfaceFormatKHR candidate =
			{
				graphicsBase::Base().AvailableSurfaceFormat_Format(i),
				graphicsBase::Base().AvailableSurfaceColorSpace(i)
			};
			outStream << std::format("[ Ch8-5 ] Surface candidate: format={} colorSpace={}\n",
				int32_t(candidate.format), int32_t(candidate.colorSpace));
			if (candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR &&
				(candidate.format == VK_FORMAT_B8G8R8A8_SRGB || candidate.format == VK_FORMAT_R8G8B8A8_SRGB))
				sdrCandidate = candidate;

			if (candidate.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
				candidate.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT)
			{
				hdrCandidate = candidate;
				hdrCandidateEncoding = 2;
			}
			if (!hdrCandidate.format &&
				(candidate.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 ||
					candidate.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32) &&
				candidate.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT)
			{
				hdrCandidate = candidate;
				hdrCandidateEncoding = 3;
			}
		}

		const bool requestHdr = hdrPreference == HdrPreference::RequestHDR ||
			(example == RenderExample::HDR && hdrPreference == HdrPreference::Auto);
		if (requestHdr && hdrCandidate.format)
		{
			selected = hdrCandidate;
			hdrOutputActive = true;
			hdrOutputEncoding = hdrCandidateEncoding;
		}
		else
		{
			selected = sdrCandidate;
			hdrOutputActive = false;
			hdrOutputEncoding = 0;
			if (requestHdr)
				outStream << "[ Ch8-5 ] HDR surface format is unavailable; using the SDR fallback.\n";
		}

		if (selected.format != current.imageFormat || selected.colorSpace != current.imageColorSpace)
		{
			if (VkResult result = graphicsBase::Base().SetSurfaceFormat(selected))
				return result;
		}
		outputIsSrgbAttachment = selected.format == VK_FORMAT_R8G8B8A8_SRGB ||
			selected.format == VK_FORMAT_B8G8R8A8_SRGB;
		outStream << std::format(
			"[ Ch8-5 ] Selected output: format={} colorSpace={} hdrActive={} encoding={} sRGB-attachment={}\n",
			int32_t(selected.format), int32_t(selected.colorSpace), hdrOutputActive,
			hdrOutputEncoding, outputIsSrgbAttachment);
		return VK_SUCCESS;
	}

	VkResult CreateColorOutputDeviceResources()
	{
		VkDescriptorSetLayoutBinding hdrBinding =
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo setLayoutCi =
		{
			.bindingCount = 1,
			.pBindings = &hdrBinding
		};
		VkResult result = colorOutputDescriptorSetLayout.Create(setLayoutCi);
		if (result) return result;
		VkPushConstantRange outputRange =
		{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(glm::vec4)
		};
		VkPipelineLayoutCreateInfo layoutCi =
		{
			.setLayoutCount = 1,
			.pSetLayouts = colorOutputDescriptorSetLayout.Address(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &outputRange
		};
		if ((result = colorOutputPipelineLayout.Create(layoutCi)) ||
			(result = CreateFullscreenSampler(hdrSampler)) ||
			(result = fullscreenVertexShader.Create("shader/Fullscreen.vert.spv")))
			return result;
		if (example == RenderExample::SRGBTest)
			return colorSpaceTestFragmentShader.Create("shader/ColorSpaceTest.frag.spv");
		if ((result = hdrSceneFragmentShader.Create("shader/HDRScene.frag.spv")) ||
			(result = toneMappingFragmentShader.Create("shader/ToneMapping.frag.spv")))
			return result;
		return VK_SUCCESS;
	}

	VkResult CreateDeviceResources()
	{
		if (IsColorOutputExample(example))
			return CreateColorOutputDeviceResources();
		if (IsAlphaExample(example))
			return CreateAlphaDeviceResources();
		if (example != RenderExample::Offscreen && !IsDepthExample(example) && !IsDeferredExample(example))
			return VK_SUCCESS;

		VkResult result = VK_SUCCESS;
		if (example == RenderExample::Offscreen)
		{
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
			if ((result = offscreenPipelineLayout.Create(offscreenLayoutCi)))
				return result;
		}

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
		VkPushConstantRange fullscreenPushRange =
		{
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(glm::vec4)
		};
		VkPipelineLayoutCreateInfo fullscreenLayoutCi =
		{
			.setLayoutCount = 1,
			.pSetLayouts = offscreenDescriptorSetLayout.Address(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &fullscreenPushRange
		};
		result = fullscreenPipelineLayout.Create(fullscreenLayoutCi);
		if (result)
			return result;
		result = CreateFullscreenSampler(offscreenSampler);
		if (result)
			return result;
		if ((result = fullscreenVertexShader.Create("shader/Fullscreen.vert.spv")) ||
			(result = fullscreenFragmentShader.Create("shader/Fullscreen.frag.spv")))
			return result;
		if (example == RenderExample::Offscreen)
		{
			if ((result = offscreenVertexShader.Create("shader/Offscreen.vert.spv")) ||
				(result = offscreenFragmentShader.Create("shader/Offscreen.frag.spv")))
				return result;
		}
		else
		{
			VkDescriptorSetLayoutBinding uniformBinding =
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount = 1,
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT
			};
			VkDescriptorSetLayoutCreateInfo depthSetLayoutCi =
			{
				.bindingCount = 1,
				.pBindings = &uniformBinding
			};
			if ((result = depthSceneDescriptorSetLayout.Create(depthSetLayoutCi)))
				return result;
			VkPipelineLayoutCreateInfo depthLayoutCi =
			{
				.setLayoutCount = 1,
				.pSetLayouts = depthSceneDescriptorSetLayout.Address()
			};
			if ((result = depthScenePipelineLayout.Create(depthLayoutCi)) ||
				(result = cubeVertexBuffer.CreateDeviceLocal(cubeVertices, sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)) ||
				(result = cubeIndexBuffer.CreateDeviceLocal(cubeIndices, sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT)) ||
				(result = depthUniformBuffer.CreateHostVisible(sizeof(DepthSceneUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT)))
				return result;
			if (IsDepthExample(example))
			{
				if ((result = depthSceneVertexShader.Create("shader/DepthScene.vert.spv")) ||
					(result = depthSceneFragmentShader.Create("shader/DepthScene.frag.spv")) ||
					(result = depthVisualizeFragmentShader.Create("shader/DepthVisualize.frag.spv")))
					return result;
			}
			else
			{
				VkDescriptorSetLayoutBinding inputBindings[4] = {};
				for (uint32_t i = 0; i < uint32_t(std::size(inputBindings)); i++)
					inputBindings[i] = { i, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1,
						VK_SHADER_STAGE_FRAGMENT_BIT, nullptr };
				VkDescriptorSetLayoutCreateInfo inputLayoutCi =
				{
					.bindingCount = uint32_t(std::size(inputBindings)),
					.pBindings = inputBindings
				};
				if ((result = deferredInputDescriptorSetLayout.Create(inputLayoutCi)))
					return result;
				VkPushConstantRange deferredPushRange =
				{
					.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
					.offset = 0,
					.size = sizeof(glm::vec4)
				};
				VkPipelineLayoutCreateInfo compositionLayoutCi =
				{
					.setLayoutCount = 1,
					.pSetLayouts = deferredInputDescriptorSetLayout.Address(),
					.pushConstantRangeCount = 1,
					.pPushConstantRanges = &deferredPushRange
				};
				if ((result = deferredCompositionPipelineLayout.Create(compositionLayoutCi)) ||
					(result = deferredGeometryVertexShader.Create("shader/DeferredGeometry.vert.spv")) ||
					(result = deferredGeometryFragmentShader.Create("shader/DeferredGeometry.frag.spv")) ||
					(result = deferredCompositionFragmentShader.Create("shader/DeferredComposition.frag.spv")))
					return result;
			}
		}
		return VK_SUCCESS;
	}

	VkResult CreateAlphaDeviceResources()
	{
		VkDescriptorSetLayoutBinding textureBinding =
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
		};
		VkDescriptorSetLayoutCreateInfo setLayoutCi =
		{
			.bindingCount = 1,
			.pBindings = &textureBinding
		};
		VkResult result = alphaDescriptorSetLayout.Create(setLayoutCi);
		if (result) return result;
		VkPushConstantRange panelRange =
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.offset = 0,
			.size = sizeof(glm::vec4)
		};
		VkPipelineLayoutCreateInfo layoutCi =
		{
			.setLayoutCount = 1,
			.pSetLayouts = alphaDescriptorSetLayout.Address(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &panelRange
		};
		if ((result = alphaPipelineLayout.Create(layoutCi)) ||
			(result = straightAlphaTexture.Create("textures/nettle_plant_diff_4k.png",
				VK_FORMAT_R8G8B8A8_UNORM, false)) ||
			(result = premultipliedAlphaTexture.Create("textures/nettle_plant_diff_4k.png",
				VK_FORMAT_R8G8B8A8_UNORM, true)) ||
			(result = alphaVertexShader.Create("shader/AlphaTest.vert.spv")) ||
			(result = alphaFragmentShader.Create("shader/AlphaTest.frag.spv")))
			return result;
		return VK_SUCCESS;
	}

	void DestroyDeviceResources()
	{
		offscreenVertexShader.Destroy();
		offscreenFragmentShader.Destroy();
		fullscreenVertexShader.Destroy();
		fullscreenFragmentShader.Destroy();
		depthSceneVertexShader.Destroy();
		depthSceneFragmentShader.Destroy();
		depthVisualizeFragmentShader.Destroy();
		deferredGeometryVertexShader.Destroy();
		deferredGeometryFragmentShader.Destroy();
		deferredCompositionFragmentShader.Destroy();
		cubeVertexBuffer.Destroy();
		cubeIndexBuffer.Destroy();
		depthUniformBuffer.Destroy();
		if (offscreenSampler)
		{
			vkDestroySampler(graphicsBase::Base().Device(), offscreenSampler, nullptr);
			offscreenSampler = VK_NULL_HANDLE;
		}
		fullscreenPipelineLayout.Destroy();
		offscreenPipelineLayout.Destroy();
		depthScenePipelineLayout.Destroy();
		deferredCompositionPipelineLayout.Destroy();
		offscreenDescriptorSetLayout.Destroy();
		depthSceneDescriptorSetLayout.Destroy();
		deferredInputDescriptorSetLayout.Destroy();
		alphaVertexShader.Destroy();
		alphaFragmentShader.Destroy();
		straightAlphaTexture.Destroy();
		premultipliedAlphaTexture.Destroy();
		alphaPipelineLayout.Destroy();
		alphaDescriptorSetLayout.Destroy();
		colorSpaceTestFragmentShader.Destroy();
		hdrSceneFragmentShader.Destroy();
		toneMappingFragmentShader.Destroy();
		if (hdrSampler)
		{
			vkDestroySampler(graphicsBase::Base().Device(), hdrSampler, nullptr);
			hdrSampler = VK_NULL_HANDLE;
		}
		colorOutputPipelineLayout.Destroy();
		colorOutputDescriptorSetLayout.Destroy();
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

	VkResult CreateDepthImage()
	{
		depthFormat = FindDepthFormat();
		if (depthFormat == VK_FORMAT_UNDEFINED)
		{
			outStream << "[ Ch8-2 ] ERROR\nNo sampled depth-attachment format is supported.\n";
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		}
		VkResult result = depthImage.Create({ windowSize.width, windowSize.height, 1 }, 1, depthFormat,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result)
			result = depthImage.CreateView(VK_IMAGE_ASPECT_DEPTH_BIT);
		return result;
	}

	VkResult CreateGBufferImages()
	{
		const VkFormatFeatureFlags colorRequired = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		if (!FormatSupports(VK_FORMAT_R8G8B8A8_UNORM, colorRequired) ||
			!FormatSupports(VK_FORMAT_R16G16B16A16_SFLOAT, colorRequired))
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		const VkExtent3D extent = { windowSize.width, windowSize.height, 1 };
		const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
		VkResult result = gbufferAlbedo.Create(extent, 1, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result) result = gbufferAlbedo.CreateView();
		if (!result) result = gbufferNormal.Create(extent, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result) result = gbufferNormal.CreateView();
		if (!result) result = gbufferPosition.Create(extent, 1, VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_IMAGE_TILING_OPTIMAL, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result) result = gbufferPosition.CreateView();
		depthFormat = FindDepthFormat();
		if (!result && depthFormat == VK_FORMAT_UNDEFINED)
			result = VK_ERROR_FORMAT_NOT_SUPPORTED;
		if (!result) result = depthImage.Create(extent, 1, depthFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result) result = depthImage.CreateView(VK_IMAGE_ASPECT_DEPTH_BIT);
		return result;
	}

	VkResult CreateDeferredRenderPass()
	{
		VkAttachmentDescription attachments[] =
		{
			{ 0, graphicsBase::Base().SwapchainCreateInfo().imageFormat, VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR },
			{ 0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 0, VK_FORMAT_R16G16B16A16_SFLOAT, VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 0, depthFormat, VK_SAMPLE_COUNT_1_BIT,
				VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
				VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }
		};
		VkAttachmentReference geometryColors[] =
		{
			{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
			{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL }
		};
		VkAttachmentReference depthReference = { 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkAttachmentReference compositionInputs[] =
		{
			{ 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }
		};
		VkAttachmentReference swapchainColor = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpasses[] =
		{
			{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount = uint32_t(std::size(geometryColors)),
				.pColorAttachments = geometryColors,
				.pDepthStencilAttachment = &depthReference
			},
			{
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = uint32_t(std::size(compositionInputs)),
				.pInputAttachments = compositionInputs,
				.colorAttachmentCount = 1,
				.pColorAttachments = &swapchainColor
			}
		};
		VkSubpassDependency dependencies[] =
		{
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass = 0,
				.dstSubpass = 1,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass = 1,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			}
		};
		VkRenderPassCreateInfo createInfo =
		{
			.attachmentCount = uint32_t(std::size(attachments)),
			.pAttachments = attachments,
			.subpassCount = uint32_t(std::size(subpasses)),
			.pSubpasses = subpasses,
			.dependencyCount = uint32_t(std::size(dependencies)),
			.pDependencies = dependencies
		};
		return deferredRenderPass.Create(createInfo);
	}

	VkResult CreateDeferredFramebuffers()
	{
		deferredFramebuffers.resize(graphicsBase::Base().SwapchainImageCount());
		for (uint32_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++)
		{
			VkImageView attachments[] =
			{
				graphicsBase::Base().SwapchainImageView(i), gbufferAlbedo.View(),
				gbufferNormal.View(), gbufferPosition.View(), depthImage.View()
			};
			VkFramebufferCreateInfo createInfo =
			{
				.renderPass = deferredRenderPass,
				.attachmentCount = uint32_t(std::size(attachments)),
				.pAttachments = attachments,
				.width = windowSize.width,
				.height = windowSize.height,
				.layers = 1
			};
			if (VkResult result = deferredFramebuffers[i].Create(createInfo))
				return result;
		}
		return VK_SUCCESS;
	}

	VkResult CreateDeferredDescriptors()
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 4 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }
		};
		VkResult result = deferredDescriptorPool.Create(2, uint32_t(std::size(poolSizes)), poolSizes);
		if (!result) result = deferredDescriptorPool.Allocate(deferredInputDescriptorSetLayout, 1, &deferredInputDescriptorSet);
		if (!result) result = deferredDescriptorPool.Allocate(depthSceneDescriptorSetLayout, 1, &depthSceneDescriptorSet);
		if (result) return result;

		VkDescriptorImageInfo imageInfos[] =
		{
			{ VK_NULL_HANDLE, gbufferAlbedo.View(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ VK_NULL_HANDLE, gbufferNormal.View(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ VK_NULL_HANDLE, gbufferPosition.View(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
			{ VK_NULL_HANDLE, depthImage.View(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }
		};
		VkDescriptorBufferInfo uniformInfo = depthUniformBuffer.DescriptorInfo(sizeof(DepthSceneUniform));
		VkWriteDescriptorSet writes[5] = {};
		for (uint32_t i = 0; i < 4; i++)
		{
			writes[i] =
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = deferredInputDescriptorSet,
				.dstBinding = i,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
				.pImageInfo = &imageInfos[i]
			};
		}
		writes[4] =
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = depthSceneDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.pBufferInfo = &uniformInfo
		};
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), uint32_t(std::size(writes)), writes, 0, nullptr);
		return VK_SUCCESS;
	}

	VkResult CreateDeferredPipelines()
	{
		VkResult result = CreateDepthScenePipeline(deferredGeometryVertexShader, deferredGeometryFragmentShader,
			depthScenePipelineLayout, deferredRenderPass, deferredGeometryPipeline, windowSize, 3);
		VkPipelineColorBlendAttachmentState opaqueBlend =
		{
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		if (!result)
			result = CreateNoVertexPipeline(fullscreenVertexShader, deferredCompositionFragmentShader,
				deferredCompositionPipelineLayout, deferredRenderPass, 1,
				deferredCompositionPipeline, windowSize, { opaqueBlend });
		return result;
	}

	VkResult CreateDepthRenderPass()
	{
		VkAttachmentDescription attachments[] =
		{
			{
				.format = offscreenColorFormat,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			},
			{
				.format = depthFormat,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			}
		};
		VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		VkSubpassDescription subpass =
		{
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorReference,
			.pDepthStencilAttachment = &depthReference
		};
		VkSubpassDependency dependencies[] =
		{
			{
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
					VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass = 0,
				.dstSubpass = VK_SUBPASS_EXTERNAL,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
					VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
					VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
			}
		};
		VkRenderPassCreateInfo createInfo =
		{
			.attachmentCount = uint32_t(std::size(attachments)),
			.pAttachments = attachments,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(std::size(dependencies)),
			.pDependencies = dependencies
		};
		return depthRenderPass.Create(createInfo);
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
		VkResult result = VK_SUCCESS;
		if (IsDepthExample(example))
		{
			VkImageView attachments[] = { offscreenColor.View(), depthImage.View() };
			VkFramebufferCreateInfo depthCi =
			{
				.renderPass = depthRenderPass,
				.attachmentCount = uint32_t(std::size(attachments)),
				.pAttachments = attachments,
				.width = windowSize.width,
				.height = windowSize.height,
				.layers = 1
			};
			result = depthFramebuffer.Create(depthCi);
		}
		else
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
			result = offscreenFramebuffer.Create(offscreenCi);
		}
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

	VkResult CreateScreenFramebuffersOnly()
	{
		screenFramebuffers.resize(graphicsBase::Base().SwapchainImageCount());
		for (uint32_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++)
		{
			VkImageView attachment = graphicsBase::Base().SwapchainImageView(i);
			VkFramebufferCreateInfo createInfo =
			{
				.renderPass = screenRenderPass,
				.attachmentCount = 1,
				.pAttachments = &attachment,
				.width = windowSize.width,
				.height = windowSize.height,
				.layers = 1
			};
			if (VkResult result = screenFramebuffers[i].Create(createInfo))
				return result;
		}
		return VK_SUCCESS;
	}

	VkResult CreateHdrTarget()
	{
		const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		if (!FormatSupports(VK_FORMAT_R16G16B16A16_SFLOAT, required))
			return VK_ERROR_FORMAT_NOT_SUPPORTED;
		VkResult result = hdrColor.Create({ windowSize.width, windowSize.height, 1 }, 1,
			VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (!result) result = hdrColor.CreateView();
		return result;
	}

	VkResult CreateHdrRenderPassAndFramebuffer()
	{
		VkAttachmentDescription attachment =
		{
			.format = VK_FORMAT_R16G16B16A16_SFLOAT,
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
		VkRenderPassCreateInfo renderPassCi =
		{
			.attachmentCount = 1,
			.pAttachments = &attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = uint32_t(std::size(dependencies)),
			.pDependencies = dependencies
		};
		VkResult result = hdrRenderPass.Create(renderPassCi);
		if (result) return result;
		VkImageView view = hdrColor.View();
		VkFramebufferCreateInfo framebufferCi =
		{
			.renderPass = hdrRenderPass,
			.attachmentCount = 1,
			.pAttachments = &view,
			.width = windowSize.width,
			.height = windowSize.height,
			.layers = 1
		};
		return hdrFramebuffer.Create(framebufferCi);
	}

	VkResult CreateColorOutputDescriptor()
	{
		VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 };
		VkResult result = colorOutputDescriptorPool.Create(1, 1, &poolSize);
		if (!result) result = colorOutputDescriptorPool.Allocate(
			colorOutputDescriptorSetLayout, 1, &colorOutputDescriptorSet);
		if (result) return result;
		VkDescriptorImageInfo imageInfo =
		{
			.sampler = hdrSampler,
			.imageView = hdrColor.View(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		VkWriteDescriptorSet write =
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = colorOutputDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &imageInfo
		};
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), 1, &write, 0, nullptr);
		return VK_SUCCESS;
	}

	VkResult CreateColorOutputPipelines()
	{
		VkPipelineColorBlendAttachmentState opaqueBlend =
		{
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		if (example == RenderExample::SRGBTest)
			return CreateNoVertexPipeline(fullscreenVertexShader, colorSpaceTestFragmentShader,
				colorOutputPipelineLayout, screenRenderPass, 0, colorSpaceTestPipeline,
				windowSize, { opaqueBlend });
		VkResult result = CreateNoVertexPipeline(fullscreenVertexShader, hdrSceneFragmentShader,
			colorOutputPipelineLayout, hdrRenderPass, 0, hdrScenePipeline, windowSize, { opaqueBlend });
		if (!result)
			result = CreateNoVertexPipeline(fullscreenVertexShader, toneMappingFragmentShader,
				colorOutputPipelineLayout, screenRenderPass, 0, toneMappingPipeline,
				windowSize, { opaqueBlend });
		return result;
	}

	VkResult CreateAlphaDescriptors()
	{
		VkDescriptorPoolSize poolSize = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 };
		VkResult result = alphaDescriptorPool.Create(2, 1, &poolSize);
		if (!result) result = alphaDescriptorPool.Allocate(alphaDescriptorSetLayout, 1, &straightAlphaDescriptorSet);
		if (!result) result = alphaDescriptorPool.Allocate(alphaDescriptorSetLayout, 1, &premultipliedAlphaDescriptorSet);
		if (result) return result;
		VkDescriptorImageInfo imageInfos[] =
		{
			straightAlphaTexture.DescriptorInfo(),
			premultipliedAlphaTexture.DescriptorInfo()
		};
		VkWriteDescriptorSet writes[] =
		{
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = straightAlphaDescriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfos[0]
			},
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = premultipliedAlphaDescriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfos[1]
			}
		};
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), uint32_t(std::size(writes)), writes, 0, nullptr);
		return VK_SUCCESS;
	}

	VkResult CreateAlphaPipelines()
	{
		VkPipelineColorBlendAttachmentState straightBlend =
		{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		VkPipelineColorBlendAttachmentState premultipliedBlend = straightBlend;
		premultipliedBlend.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		VkResult result = CreateNoVertexPipeline(alphaVertexShader, alphaFragmentShader,
			alphaPipelineLayout, screenRenderPass, 0, straightAlphaPipeline, windowSize, { straightBlend });
		if (!result) result = CreateNoVertexPipeline(alphaVertexShader, alphaFragmentShader,
			alphaPipelineLayout, screenRenderPass, 0, incorrectPremultipliedPipeline,
			windowSize, { premultipliedBlend });
		if (!result) result = CreateNoVertexPipeline(alphaVertexShader, alphaFragmentShader,
			alphaPipelineLayout, screenRenderPass, 0, premultipliedAlphaPipeline,
			windowSize, { premultipliedBlend });
		return result;
	}

	VkResult CreateDescriptors()
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IsDepthExample(example) ? 2u : 1u },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, IsDepthExample(example) ? 1u : 0u }
		};
		const uint32_t poolSizeCount = IsDepthExample(example) ? 2u : 1u;
		const uint32_t maxSets = IsDepthExample(example) ? 3u : 1u;
		VkResult result = offscreenDescriptorPool.Create(maxSets, poolSizeCount, poolSizes);
		if (result)
			return result;
		result = offscreenDescriptorPool.Allocate(offscreenDescriptorSetLayout, 1, &offscreenDescriptorSet);
		if (result)
			return result;
		VkDescriptorImageInfo colorImageInfo =
		{
			.sampler = offscreenSampler,
			.imageView = offscreenColor.View(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		std::vector<VkWriteDescriptorSet> writes;
		writes.push_back(
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = offscreenDescriptorSet,
			.dstBinding = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &colorImageInfo
		});

		VkDescriptorImageInfo depthImageInfo = {};
		VkDescriptorBufferInfo uniformInfo = {};
		if (IsDepthExample(example))
		{
			if ((result = offscreenDescriptorPool.Allocate(offscreenDescriptorSetLayout, 1, &depthDescriptorSet)) ||
				(result = offscreenDescriptorPool.Allocate(depthSceneDescriptorSetLayout, 1, &depthSceneDescriptorSet)))
				return result;
			depthImageInfo =
			{
				.sampler = offscreenSampler,
				.imageView = depthImage.View(),
				.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
			};
			uniformInfo = depthUniformBuffer.DescriptorInfo(sizeof(DepthSceneUniform));
			writes.push_back(
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = depthDescriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &depthImageInfo
			});
			writes.push_back(
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = depthSceneDescriptorSet,
				.dstBinding = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &uniformInfo
			});
		}
		vkUpdateDescriptorSets(graphicsBase::Base().Device(), uint32_t(writes.size()), writes.data(), 0, nullptr);
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

	VkResult CreateDepthPipelines()
	{
		VkPipelineColorBlendAttachmentState opaqueBlend =
		{
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		VkResult result = CreateDepthScenePipeline(depthSceneVertexShader, depthSceneFragmentShader,
			depthScenePipelineLayout, depthRenderPass, depthScenePipeline, windowSize);
		if (!result)
			result = CreateNoVertexPipeline(fullscreenVertexShader, fullscreenFragmentShader,
				fullscreenPipelineLayout, screenRenderPass, 0, fullscreenPipeline, windowSize, { opaqueBlend });
		if (!result)
			result = CreateNoVertexPipeline(fullscreenVertexShader, depthVisualizeFragmentShader,
				fullscreenPipelineLayout, screenRenderPass, 0, depthVisualizePipeline, windowSize, { opaqueBlend });
		return result;
	}

	VkResult UpdateDepthUniform()
	{
		const float time = float(glfwGetTime());
		DepthSceneUniform scene = {};
		scene.view = glm::lookAt(glm::vec3(4.5f, 3.2f, 7.0f), glm::vec3(0.0f, 0.0f, -1.0f),
			glm::vec3(0.0f, 1.0f, 0.0f));
		scene.projection = glm::perspective(glm::radians(55.0f),
			float(windowSize.width) / float(windowSize.height), depthNearPlane, depthFarPlane);
		scene.projection[1][1] *= -1.0f;
		scene.models[0] = glm::translate(glm::mat4(1.0f), glm::vec3(-1.4f, -0.2f, 0.0f)) *
			glm::rotate(glm::mat4(1.0f), time * 0.45f, glm::vec3(0.0f, 1.0f, 0.0f));
		scene.models[1] = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.35f, -1.7f)) *
			glm::rotate(glm::mat4(1.0f), -time * 0.3f, glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f))) *
			glm::scale(glm::mat4(1.0f), glm::vec3(1.25f));
		scene.models[2] = glm::translate(glm::mat4(1.0f), glm::vec3(1.25f, -0.35f, -3.0f)) *
			glm::rotate(glm::mat4(1.0f), time * 0.25f, glm::vec3(1.0f, 0.0f, 1.0f));
		scene.nearFarMode = { depthNearPlane, depthFarPlane,
			example == RenderExample::DepthLinear ? 1.0f : 0.0f, 0.0f };
		return depthUniformBuffer.Write(&scene, sizeof(scene));
	}

	void RecordDeferredFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue clearValues[5] = {};
		clearValues[0].color = { { 0.01f, 0.015f, 0.025f, 1.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 1.0f, 1.0f } };
		clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		clearValues[4].depthStencil = { 1.0f, 0 };
		BeginRenderPass(commandBuffer, deferredRenderPass, deferredFramebuffers[imageIndex],
			windowSize, clearValues);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredGeometryPipeline);
		VkBuffer vertexBuffer = cubeVertexBuffer;
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
		vkCmdBindIndexBuffer(commandBuffer, cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthScenePipelineLayout,
			0, 1, &depthSceneDescriptorSet, 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, uint32_t(std::size(cubeIndices)), 3, 0, 0, 0);

		vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredCompositionPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, deferredCompositionPipelineLayout,
			0, 1, &deferredInputDescriptorSet, 0, nullptr);
		float debugMode = 0.0f;
		if (example == RenderExample::DeferredAlbedo) debugMode = 1.0f;
		else if (example == RenderExample::DeferredNormal) debugMode = 2.0f;
		else if (example == RenderExample::DeferredPosition) debugMode = 3.0f;
		const glm::vec4 display = { debugMode, 0.0f, 0.0f, 0.0f };
		vkCmdPushConstants(commandBuffer, deferredCompositionPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(display), &display);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		vkCmdEndRenderPass(commandBuffer);
	}

	void RecordAlphaFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue clear = {};
		clear.color = { { 0.82f, 0.82f, 0.82f, 1.0f } };
		screenRenderPass.CmdBegin(commandBuffer, screenFramebuffers[imageIndex], { {}, windowSize }, clear);
		const VkPipeline pipelines[] =
		{
			straightAlphaPipeline,
			incorrectPremultipliedPipeline,
			premultipliedAlphaPipeline
		};
		const VkDescriptorSet descriptorSets[] =
		{
			straightAlphaDescriptorSet,
			straightAlphaDescriptorSet,
			premultipliedAlphaDescriptorSet
		};
		for (uint32_t panel = 0; panel < 3; panel++)
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines[panel]);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, alphaPipelineLayout,
				0, 1, &descriptorSets[panel], 0, nullptr);
			const glm::vec4 push = { float(panel), 0.0f, 0.0f, 0.0f };
			vkCmdPushConstants(commandBuffer, alphaPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
				0, sizeof(push), &push);
			vkCmdDraw(commandBuffer, 6, 1, 0, 0);
		}
		screenRenderPass.CmdEnd(commandBuffer);
	}

	void RecordColorOutputFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		if (example == RenderExample::SRGBTest)
		{
			VkClearValue clear = {};
			clear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			screenRenderPass.CmdBegin(commandBuffer, screenFramebuffers[imageIndex], { {}, windowSize }, clear);
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, colorSpaceTestPipeline);
			const glm::vec4 push = { outputIsSrgbAttachment ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f };
			vkCmdPushConstants(commandBuffer, colorOutputPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(push), &push);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0);
			screenRenderPass.CmdEnd(commandBuffer);
			return;
		}

		VkClearValue hdrClear = {};
		hdrClear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		hdrRenderPass.CmdBegin(commandBuffer, hdrFramebuffer, { {}, windowSize }, hdrClear);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdrScenePipeline);
		const glm::vec4 scenePush = { float(glfwGetTime()), 0.0f, 0.0f, 0.0f };
		vkCmdPushConstants(commandBuffer, colorOutputPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(scenePush), &scenePush);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		hdrRenderPass.CmdEnd(commandBuffer);

		VkClearValue screenClear = {};
		screenClear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		screenRenderPass.CmdBegin(commandBuffer, screenFramebuffers[imageIndex], { {}, windowSize }, screenClear);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, toneMappingPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, colorOutputPipelineLayout,
			0, 1, &colorOutputDescriptorSet, 0, nullptr);
		const float mode = hdrOutputActive ? float(hdrOutputEncoding) : (outputIsSrgbAttachment ? 1.0f : 0.0f);
		const glm::vec4 outputPush = { mode, 1.0f, 0.0f, 0.0f };
		vkCmdPushConstants(commandBuffer, colorOutputPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
			0, sizeof(outputPush), &outputPush);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		screenRenderPass.CmdEnd(commandBuffer);
	}

	void RecordDepthFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue depthClears[2] = {};
		depthClears[0].color = { { 0.02f, 0.025f, 0.04f, 1.0f } };
		depthClears[1].depthStencil = { 1.0f, 0 };
		BeginRenderPass(commandBuffer, depthRenderPass, depthFramebuffer, windowSize, depthClears);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthScenePipeline);
		VkBuffer vertexBuffer = cubeVertexBuffer;
		VkDeviceSize vertexOffset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &vertexOffset);
		vkCmdBindIndexBuffer(commandBuffer, cubeIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, depthScenePipelineLayout,
			0, 1, &depthSceneDescriptorSet, 0, nullptr);
		vkCmdDrawIndexed(commandBuffer, uint32_t(std::size(cubeIndices)), 3, 0, 0, 0);
	vkCmdEndRenderPass(commandBuffer);

		VkClearValue screenClear = {};
		screenClear.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
		screenRenderPass.CmdBegin(commandBuffer, screenFramebuffers[imageIndex], { {}, windowSize }, screenClear);
		const bool visualizeDepth = example == RenderExample::DepthRaw || example == RenderExample::DepthLinear;
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			visualizeDepth ? VkPipeline(depthVisualizePipeline) : VkPipeline(fullscreenPipeline));
		VkDescriptorSet set = visualizeDepth ? depthDescriptorSet : offscreenDescriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenPipelineLayout,
			0, 1, &set, 0, nullptr);
		if (visualizeDepth)
		{
			const glm::vec4 params = { depthNearPlane, depthFarPlane,
				example == RenderExample::DepthLinear ? 1.0f : 0.0f, 0.0f };
			vkCmdPushConstants(commandBuffer, fullscreenPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT,
				0, sizeof(params), &params);
		}
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		screenRenderPass.CmdEnd(commandBuffer);
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
