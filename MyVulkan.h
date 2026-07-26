#pragma once
#include "VKBase.h"
#include <cstring>

using namespace vulkan;

// windowSize: 交换链图像尺寸的别名，帧缓冲、视口和裁剪范围都使用这个尺寸。
inline const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan
{
using namespace vulkan;

class shaderModule
{
private:
	// VkShaderModule: 保存已经编译成 SPIR-V 的着色器代码，供图形管线创建时使用。
	VkShaderModule handle = VK_NULL_HANDLE;

public:
	shaderModule() = default;
	shaderModule(const char* filepath) { Create(filepath); }
	shaderModule(shaderModule&& other) noexcept { MoveHandle; }
	shaderModule& operator=(shaderModule&& other) noexcept { this->~shaderModule(); MoveHandle; return *this; }
	shaderModule(const shaderModule&) = delete;
	shaderModule& operator=(const shaderModule&) = delete;
	~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	result_t Create(const char* filepath)
	{
		Destroy();

		std::ifstream file(filepath, std::ios::ate | std::ios::binary);
		if (!file)
		{
			outStream << std::format("[ shaderModule ] ERROR\nFailed to open shader file: {}\n", filepath);
			return VK_ERROR_UNKNOWN;
		}

		size_t fileSize = size_t(file.tellg());
		if (fileSize == 0 || fileSize % sizeof(uint32_t))
		{
			outStream << std::format("[ shaderModule ] ERROR\nInvalid SPIR-V file size: {}\n", filepath);
			return VK_ERROR_UNKNOWN;
		}

		std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(code.data()), fileSize);
		if (!file)
		{
			outStream << std::format("[ shaderModule ] ERROR\nFailed to read shader file: {}\n", filepath);
			return VK_ERROR_UNKNOWN;
		}

		// VkShaderModuleCreateInfo: 指定 SPIR-V 字节码的大小和数据地址，用于创建着色器模组。
		VkShaderModuleCreateInfo createInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = fileSize,
			.pCode = code.data()
		};
		VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
		if (result)
			outStream << std::format("[ shaderModule ] ERROR\nFailed to create a shader module!\nError code: {}\n", string_VkResult(result));
		return result;
	}

	void Destroy()
	{
		DestroyHandleBy(vkDestroyShaderModule);
	}

	VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const
	{
		// VkPipelineShaderStageCreateInfo: 指定当前管线阶段使用哪个着色器模组以及入口函数。
		VkPipelineShaderStageCreateInfo createInfo =
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = stage,
			.module = handle,
			.pName = entry
		};
		return createInfo;
	}
};

class bufferMemory
{
private:
	VkBuffer handle = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;

	static uint32_t FindMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags requiredProperties)
	{
		const auto& memoryProperties = graphicsBase::Base().PhysicalDeviceMemoryProperties();
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
			if ((memoryTypeBits & (1u << i)) &&
				(memoryProperties.memoryTypes[i].propertyFlags & requiredProperties) == requiredProperties)
				return i;
		return UINT32_MAX;
	}

	static VkResult CreateBufferAndMemory(VkDeviceSize size, VkBufferUsageFlags usage,
		VkMemoryPropertyFlags memoryProperties, VkBuffer& buffer, VkDeviceMemory& deviceMemory)
	{
		VkBufferCreateInfo bufferCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = size,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		VkResult result = vkCreateBuffer(graphicsBase::Base().Device(), &bufferCreateInfo, nullptr, &buffer);
		if (result)
		{
			outStream << std::format("[ bufferMemory ] ERROR\nFailed to create a buffer!\nError code: {}\n", string_VkResult(result));
			return result;
		}

		VkMemoryRequirements memoryRequirements;
		vkGetBufferMemoryRequirements(graphicsBase::Base().Device(), buffer, &memoryRequirements);
		const uint32_t memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryProperties);
		if (memoryTypeIndex == UINT32_MAX)
		{
			outStream << "[ bufferMemory ] ERROR\nFailed to find a suitable memory type!\n";
			vkDestroyBuffer(graphicsBase::Base().Device(), buffer, nullptr);
			buffer = VK_NULL_HANDLE;
			return VK_ERROR_FEATURE_NOT_PRESENT;
		}

		VkMemoryAllocateInfo allocateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.allocationSize = memoryRequirements.size,
			.memoryTypeIndex = memoryTypeIndex
		};
		result = vkAllocateMemory(graphicsBase::Base().Device(), &allocateInfo, nullptr, &deviceMemory);
		if (result)
		{
			outStream << std::format("[ bufferMemory ] ERROR\nFailed to allocate buffer memory!\nError code: {}\n", string_VkResult(result));
			vkDestroyBuffer(graphicsBase::Base().Device(), buffer, nullptr);
			buffer = VK_NULL_HANDLE;
			return result;
		}

		result = vkBindBufferMemory(graphicsBase::Base().Device(), buffer, deviceMemory, 0);
		if (result)
		{
			outStream << std::format("[ bufferMemory ] ERROR\nFailed to bind buffer memory!\nError code: {}\n", string_VkResult(result));
			vkDestroyBuffer(graphicsBase::Base().Device(), buffer, nullptr);
			vkFreeMemory(graphicsBase::Base().Device(), deviceMemory, nullptr);
			buffer = VK_NULL_HANDLE;
			deviceMemory = VK_NULL_HANDLE;
		}
		return result;
	}

	static VkResult CopyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size)
	{
		VkCommandPool copyCommandPool = VK_NULL_HANDLE;
		VkCommandPoolCreateInfo poolCreateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = graphicsBase::Base().QueueFamilyIndex_Graphics()
		};
		VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &poolCreateInfo, nullptr, &copyCommandPool);
		if (result)
			return result;

		VkCommandBuffer copyCommandBuffer = VK_NULL_HANDLE;
		VkCommandBufferAllocateInfo allocateInfo =
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = copyCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1
		};
		result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, &copyCommandBuffer);
		if (!result)
		{
			VkCommandBufferBeginInfo beginInfo =
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
			};
			result = vkBeginCommandBuffer(copyCommandBuffer, &beginInfo);
		}
		if (!result)
		{
			VkBufferCopy copyRegion = { .size = size };
			vkCmdCopyBuffer(copyCommandBuffer, source, destination, 1, &copyRegion);
			result = vkEndCommandBuffer(copyCommandBuffer);
		}
		if (!result)
		{
			VkSubmitInfo submitInfo =
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &copyCommandBuffer
			};
			result = vkQueueSubmit(graphicsBase::Base().Queue_Graphics(), 1, &submitInfo, VK_NULL_HANDLE);
		}
		if (!result)
			result = vkQueueWaitIdle(graphicsBase::Base().Queue_Graphics());

		vkDestroyCommandPool(graphicsBase::Base().Device(), copyCommandPool, nullptr);
		if (result)
			outStream << std::format("[ bufferMemory ] ERROR\nFailed to transfer buffer data!\nError code: {}\n", string_VkResult(result));
		return result;
	}

public:
	bufferMemory() = default;
	bufferMemory(const bufferMemory&) = delete;
	bufferMemory& operator=(const bufferMemory&) = delete;
	~bufferMemory() { Destroy(); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	VkResult CreateDeviceLocal(const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
	{
		Destroy();

		VkBuffer stagingBuffer = VK_NULL_HANDLE;
		VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
		VkResult result = CreateBufferAndMemory(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingMemory);
		if (result)
			return result;

		void* mappedMemory = nullptr;
		result = vkMapMemory(graphicsBase::Base().Device(), stagingMemory, 0, size, 0, &mappedMemory);
		if (!result)
		{
			std::memcpy(mappedMemory, data, size_t(size));
			vkUnmapMemory(graphicsBase::Base().Device(), stagingMemory);
			result = CreateBufferAndMemory(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, handle, memory);
		}
		if (!result)
			result = CopyBuffer(stagingBuffer, handle, size);

		vkDestroyBuffer(graphicsBase::Base().Device(), stagingBuffer, nullptr);
		vkFreeMemory(graphicsBase::Base().Device(), stagingMemory, nullptr);

		if (result)
			Destroy();
		return result;
	}

	void Destroy()
	{
		if (handle)
		{
			vkDestroyBuffer(graphicsBase::Base().Device(), handle, nullptr);
			handle = VK_NULL_HANDLE;
		}
		if (memory)
		{
			vkFreeMemory(graphicsBase::Base().Device(), memory, nullptr);
			memory = VK_NULL_HANDLE;
		}
	}
};

class pipelineLayout
{
private:
	// VkPipelineLayout: 描述着色器可访问的描述符集布局和 push constant 范围。
	VkPipelineLayout handle = VK_NULL_HANDLE;

public:
	pipelineLayout() = default;
	pipelineLayout(pipelineLayout&& other) noexcept { MoveHandle; }
	pipelineLayout& operator=(pipelineLayout&& other) noexcept { this->~pipelineLayout(); MoveHandle; return *this; }
	pipelineLayout(const pipelineLayout&) = delete;
	pipelineLayout& operator=(const pipelineLayout&) = delete;
	~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	result_t Create(VkPipelineLayoutCreateInfo& createInfo)
	{
		Destroy();

		// VkPipelineLayoutCreateInfo: 本节不使用描述符和 push constant，因此保持空布局。
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
		if (result)
			outStream << std::format("[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: {}\n", string_VkResult(result));
		return result;
	}

	void Destroy()
	{
		DestroyHandleBy(vkDestroyPipelineLayout);
	}
};

struct graphicsPipelineCreateInfoPack
{
	// VkGraphicsPipelineCreateInfo: 汇总 shader 阶段、固定功能状态、管线布局和渲染通道来创建图形管线。
	VkGraphicsPipelineCreateInfo createInfo =
	{
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO
	};

	std::vector<VkVertexInputBindingDescription> vertexInputBindings;
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

	// VkPipelineVertexInputStateCreateInfo: 描述顶点缓冲区中的数据步长，以及 shader 各输入位置如何读取属性。
	VkPipelineVertexInputStateCreateInfo vertexInputStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	// VkPipelineInputAssemblyStateCreateInfo: 描述顶点如何组成图元，本节使用三角形列表。
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};

	// VkViewport: 描述 NDC 到 framebuffer 像素坐标的映射区域。
	std::vector<VkViewport> viewports;

	// VkRect2D: 描述裁剪范围，只有范围内的片元才会写入颜色附件。
	std::vector<VkRect2D> scissors;

	// VkPipelineViewportStateCreateInfo: 指向视口数组和裁剪矩形数组。
	VkPipelineViewportStateCreateInfo viewportStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO
	};

	// VkPipelineRasterizationStateCreateInfo: 描述图元光栅化方式，例如填充模式、剔除模式和线宽。
	VkPipelineRasterizationStateCreateInfo rasterizationStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = VK_CULL_MODE_NONE,
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.lineWidth = 1.f
	};

	// VkPipelineMultisampleStateCreateInfo: 描述多重采样状态，本节每个像素只采样一次。
	VkPipelineMultisampleStateCreateInfo multisampleStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
	};

	// VkPipelineColorBlendAttachmentState: 描述单个颜色附件的写入遮罩和混合方式。
	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;

	// VkPipelineColorBlendStateCreateInfo: 指向所有颜色附件的混合状态。
	VkPipelineColorBlendStateCreateInfo colorBlendStateCi =
	{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
	};

	void UpdateAllArrays()
	{
		vertexInputStateCi.vertexBindingDescriptionCount = uint32_t(vertexInputBindings.size());
		vertexInputStateCi.pVertexBindingDescriptions = vertexInputBindings.data();
		vertexInputStateCi.vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size());
		vertexInputStateCi.pVertexAttributeDescriptions = vertexInputAttributes.data();

		viewportStateCi.viewportCount = uint32_t(viewports.size());
		viewportStateCi.pViewports = viewports.data();
		viewportStateCi.scissorCount = uint32_t(scissors.size());
		viewportStateCi.pScissors = scissors.data();

		colorBlendStateCi.attachmentCount = uint32_t(colorBlendAttachmentStates.size());
		colorBlendStateCi.pAttachments = colorBlendAttachmentStates.data();

		createInfo.pVertexInputState = &vertexInputStateCi;
		createInfo.pInputAssemblyState = &inputAssemblyStateCi;
		createInfo.pViewportState = &viewportStateCi;
		createInfo.pRasterizationState = &rasterizationStateCi;
		createInfo.pMultisampleState = &multisampleStateCi;
		createInfo.pColorBlendState = &colorBlendStateCi;
	}
};

class pipeline
{
private:
	// VkPipeline: 保存已经固定下来的图形管线状态，用于录制绘制命令前绑定。
	VkPipeline handle = VK_NULL_HANDLE;

public:
	pipeline() = default;
	pipeline(pipeline&& other) noexcept { MoveHandle; }
	pipeline& operator=(pipeline&& other) noexcept { this->~pipeline(); MoveHandle; return *this; }
	pipeline(const pipeline&) = delete;
	pipeline& operator=(const pipeline&) = delete;
	~pipeline() { DestroyHandleBy(vkDestroyPipeline); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	result_t Create(VkGraphicsPipelineCreateInfo& createInfo)
	{
		Destroy();

		// VkPipelineCache: 本节不使用管线缓存，因此传入 VK_NULL_HANDLE。
		VkResult result = vkCreateGraphicsPipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
		if (result)
			outStream << std::format("[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: {}\n", string_VkResult(result));
		return result;
	}

	result_t Create(graphicsPipelineCreateInfoPack& createInfoPack)
	{
		return Create(createInfoPack.createInfo);
	}

	void Destroy()
	{
		DestroyHandleBy(vkDestroyPipeline);
	}
};

class renderPass
{
private:
	// VkRenderPass: 描述渲染通道中的附件格式、子通道，以及子通道之间的同步关系。
	VkRenderPass handle = VK_NULL_HANDLE;

public:
	renderPass() = default;
	renderPass(renderPass&& other) noexcept { MoveHandle; }
	renderPass& operator=(renderPass&& other) noexcept { this->~renderPass(); MoveHandle; return *this; }
	renderPass(const renderPass&) = delete;
	renderPass& operator=(const renderPass&) = delete;
	~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	result_t Create(VkRenderPassCreateInfo& createInfo)
	{
		Destroy();

		// VkRenderPassCreateInfo: 指定渲染通道使用的附件、子通道和子通道依赖。
		createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
		if (result)
			outStream << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", string_VkResult(result));
		return result;
	}

	void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, VkClearValue clearValue) const
	{
		// VkRenderPassBeginInfo: 指定命令缓冲区开始渲染通道时使用的渲染通道、帧缓冲、渲染区域和清屏值。
		VkRenderPassBeginInfo beginInfo =
		{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = handle,
			.framebuffer = framebuffer,
			.renderArea = renderArea,
			.clearValueCount = 1,
			.pClearValues = &clearValue
		};
		vkCmdBeginRenderPass(commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void CmdEnd(VkCommandBuffer commandBuffer) const
	{
		vkCmdEndRenderPass(commandBuffer);
	}

	void Destroy()
	{
		DestroyHandleBy(vkDestroyRenderPass);
	}
};

class framebuffer
{
private:
	// VkFramebuffer: 将一个渲染通道所需的实际图像视图附件组合在一起。
	VkFramebuffer handle = VK_NULL_HANDLE;

public:
	framebuffer() = default;
	framebuffer(framebuffer&& other) noexcept { MoveHandle; }
	framebuffer& operator=(framebuffer&& other) noexcept { this->~framebuffer(); MoveHandle; return *this; }
	framebuffer(const framebuffer&) = delete;
	framebuffer& operator=(const framebuffer&) = delete;
	~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }

	DefineHandleTypeOperator;
	DefineAddressFunction;

	result_t Create(VkFramebufferCreateInfo& createInfo)
	{
		Destroy();

		// VkFramebufferCreateInfo: 指定帧缓冲所属渲染通道、附件图像视图、尺寸和层数。
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
		if (result)
			outStream << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", string_VkResult(result));
		return result;
	}

	void Destroy()
	{
		DestroyHandleBy(vkDestroyFramebuffer);
	}
};

struct renderPassWithFramebuffers
{
	renderPass renderPass;
	std::vector<framebuffer> framebuffers;
};

const auto& CreateRpwf_Screen()
{
	static renderPassWithFramebuffers rpwf;
	static bool initialized = false;
	if (initialized)
		return rpwf;

	// VkAttachmentDescription: 描述交换链图像作为颜色附件时的格式、采样数、加载/存储行为和布局转换。
	VkAttachmentDescription attachmentDescription =
	{
		.format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
		.samples = VK_SAMPLE_COUNT_1_BIT,
		.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	};

	// VkAttachmentReference: 指定子通道使用第 0 个附件，并在子通道中将它作为颜色附件使用。
	VkAttachmentReference attachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

	// VkSubpassDescription: 描述唯一的图形子通道，以及它使用的颜色附件。
	VkSubpassDescription subpassDescription =
	{
		.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
		.colorAttachmentCount = 1,
		.pColorAttachments = &attachmentReference
	};

	// VkSubpassDependency: 覆盖渲染通道开始时的隐式依赖，确保颜色附件写入发生在正确的管线阶段。
	VkSubpassDependency subpassDependency =
	{
		.srcSubpass = VK_SUBPASS_EXTERNAL,
		.dstSubpass = 0,
		.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		.srcAccessMask = 0,
		.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
	};

	// VkRenderPassCreateInfo: 将颜色附件、子通道和子通道依赖组合成一个渲染通道创建描述。
	VkRenderPassCreateInfo renderPassCreateInfo =
	{
		.attachmentCount = 1,
		.pAttachments = &attachmentDescription,
		.subpassCount = 1,
		.pSubpasses = &subpassDescription,
		.dependencyCount = 1,
		.pDependencies = &subpassDependency
	};
	rpwf.renderPass.Create(renderPassCreateInfo);

	auto CreateFramebuffers = [] {
		rpwf.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());

		// VkFramebufferCreateInfo: 每张交换链图像都对应一个只含一个颜色附件的帧缓冲。
		VkFramebufferCreateInfo framebufferCreateInfo =
		{
			.renderPass = rpwf.renderPass,
			.attachmentCount = 1,
			.width = windowSize.width,
			.height = windowSize.height,
			.layers = 1
		};
		for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++)
		{
			// VkImageView: 当前交换链图像对应的图像视图，作为帧缓冲的颜色附件。
			VkImageView attachment = graphicsBase::Base().SwapchainImageView(uint32_t(i));
			framebufferCreateInfo.pAttachments = &attachment;
			rpwf.framebuffers[i].Create(framebufferCreateInfo);
		}
	};

	auto DestroyFramebuffers = [] {
		rpwf.framebuffers.clear();
	};

	CreateFramebuffers();
	graphicsBase::Base().AddCallback_CreateSwapchain(CreateFramebuffers);
	graphicsBase::Base().AddCallback_DestroySwapchain(DestroyFramebuffers);
	graphicsBase::Base().AddCallback_DestroyDevice([] { rpwf.renderPass.Destroy(); });
	initialized = true;
	return rpwf;
}

// 对象依赖关系总结：
// 1. VkShaderModule 依赖 glslc 编译出的 SPIR-V 文件，VkPipelineShaderStageCreateInfo 依赖 VkShaderModule。
// 2. VkPipelineLayout 描述 shader 可访问资源；本节 shader 没有资源输入，所以它是空布局。
// 3. VkRenderPass 依赖交换链图像格式，因为颜色附件格式必须与交换链图像格式一致。
// 4. VkFramebuffer 依赖 VkRenderPass、交换链图像视图 VkImageView，以及交换链图像尺寸 windowSize。
// 5. VkPipeline 依赖 VkShaderModule、VkPipelineLayout、VkRenderPass，以及视口/裁剪范围等固定功能状态。
// 6. 交换链重建时，旧 VkFramebuffer 和依赖 windowSize 的 VkPipeline 必须销毁，再基于新的交换链尺寸重建。
// 7. 命令缓冲区开始渲染通道时，需要同时指定 VkRenderPass、当前交换链图像对应的 VkFramebuffer、渲染区域和清屏值。
}
