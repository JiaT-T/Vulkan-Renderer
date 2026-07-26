#include "RenderFeatureRenderer.h"
#include "AlphaBlendingRenderer.h"
#include "ColorOutputRenderer.h"
#include "DeferredRenderer.h"
#include "DepthRenderer.h"
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
	uint32_t colorAttachmentCount = 1, bool enableDepth = true)
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
	pack.depthStencilStateCi.depthTestEnable = enableDepth;
	pack.depthStencilStateCi.depthWriteEnable = enableDepth;
	pack.depthStencilStateCi.depthCompareOp = VK_COMPARE_OP_LESS;
	pack.depthStencilStateCi.maxDepthBounds = 1.0f;
	for (uint32_t i = 0; i < colorAttachmentCount; i++)
		pack.colorBlendAttachmentStates.push_back(
			{ .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT });
	pack.UpdateAllArrays();
	return output.Create(pack);
}

class RenderFeatureRenderer;
RenderFeatureRenderer* activeRenderer = nullptr;

class RenderFeatureRenderer
{
public:
	RenderFeatureRenderer(RenderExample example, HdrPreference hdrPreference)
		: example(example), hdrPreference(hdrPreference)
	{
		activeRenderer = this;
	}

	~RenderFeatureRenderer()
	{
		DestroySwapchainResources();
		DestroyDeviceResources();
		activeRenderer = nullptr;
	}

	VkResult Initialize()
	{
		VkResult result = ColorOutputRenderer::Supports(example) ? ConfigureOutputSurface() : VK_SUCCESS;
		if (!result)
			result = CreateDeviceResources();
		if (result)
			return result;
		RegisterSwapchainCallbacks();
		return CreateSwapchainResources();
	}

	int Run()
	{
		if (example != RenderExample::Offscreen && !DepthRenderer::Supports(example) &&
			!DeferredRenderer::Supports(example) && !AlphaBlendingRenderer::Supports(example) &&
			!ColorOutputRenderer::Supports(example))
		{
			outStream << "[ RenderFeature ] ERROR\nThe requested rendering feature has not been initialized.\n";
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
			if (DepthRenderer::Supports(example) || DeferredRenderer::Supports(example))
			{
				if (VkResult result = UpdateDepthUniform())
					return int(result);
			}

			if (VkResult result = commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT))
				return int(result);
			if (example == RenderExample::Offscreen)
				RecordOffscreenFrame(commandBuffer, imageIndex);
			else if (DeferredRenderer::Supports(example))
				RecordDeferredFrame(commandBuffer, imageIndex);
			else if (AlphaBlendingRenderer::Supports(example))
				RecordAlphaFrame(commandBuffer, imageIndex);
			else if (ColorOutputRenderer::Supports(example))
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
		if (ColorOutputRenderer::Supports(example))
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
		else if (AlphaBlendingRenderer::Supports(example))
		{
			result = CreateScreenRenderPass();
			if (!result) result = CreateScreenFramebuffersOnly();
			if (!result) result = CreateAlphaDescriptors();
			if (!result) result = CreateAlphaPipelines();
		}
		else if (DeferredRenderer::Supports(example))
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
			if (!result && DepthRenderer::Supports(example)) result = CreateDepthImage();
			if (!result && DepthRenderer::Supports(example)) result = CreateDepthRenderPass();
			else if (!result) result = CreateOffscreenRenderPass();
			if (!result) result = CreateScreenRenderPass();
			if (!result) result = CreateFramebuffers();
			if (!result) result = CreateDescriptors();
			if (!result) result = DepthRenderer::Supports(example) ? CreateDepthPipelines() : CreateOffscreenPipelines();
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
		depthDisabledPipeline.Destroy();
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
	pipeline depthDisabledPipeline;
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

#include "OffscreenRenderer.inl"
#include "DepthRenderer.inl"
#include "DeferredRenderer.inl"
#include "AlphaBlendingRenderer.inl"
#include "ColorOutputRenderer.inl"



	VkResult CreateDeviceResources()
	{
		if (ColorOutputRenderer::Supports(example))
			return CreateColorOutputDeviceResources();
		if (AlphaBlendingRenderer::Supports(example))
			return CreateAlphaDeviceResources();
		if (example != RenderExample::Offscreen && !DepthRenderer::Supports(example) &&
			!DeferredRenderer::Supports(example))
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
			if (DepthRenderer::Supports(example))
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
		if (DepthRenderer::Supports(example))
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







	VkResult CreateDescriptors()
	{
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, DepthRenderer::Supports(example) ? 2u : 1u },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, DepthRenderer::Supports(example) ? 1u : 0u }
		};
		const uint32_t poolSizeCount = DepthRenderer::Supports(example) ? 2u : 1u;
		const uint32_t maxSets = DepthRenderer::Supports(example) ? 3u : 1u;
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
		if (DepthRenderer::Supports(example))
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
};

void CreateRenderFeatureSwapchainResources()
{
	if (activeRenderer && activeRenderer->CreateSwapchainResources())
		outStream << "[ RenderFeature ] ERROR\nFailed to recreate swapchain-dependent rendering resources.\n";
}

void DestroyRenderFeatureSwapchainResources()
{
	if (activeRenderer)
		activeRenderer->DestroySwapchainResources();
}

void RenderFeatureRenderer::RegisterSwapchainCallbacks()
{
	if (callbacksRegistered)
		return;
	graphicsBase::Base().AddCallback_CreateSwapchain(CreateRenderFeatureSwapchainResources);
	graphicsBase::Base().AddCallback_DestroySwapchain(DestroyRenderFeatureSwapchainResources);
	callbacksRegistered = true;
}
}

int RunRenderFeatureRenderer(RenderExample example, HdrPreference hdrPreference)
{
	outStream << std::format("[ RenderFeature ] Example: {}\n", RenderExampleName(example));
	RenderFeatureRenderer renderer(example, hdrPreference);
	if (VkResult result = renderer.Initialize())
		return int(result);
	return renderer.Run();
}



