#pragma once
// Included inside RenderFeatureRenderer; contains offscreen-rendering resource and command methods.

	VkResult CreateOffscreenImage()
	{
		const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
			VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
		if (!FormatSupports(offscreenColorFormat, required))
		{
			outStream << "[ OffscreenRendering ] ERROR\nR8G8B8A8_UNORM does not support the required offscreen usages.\n";
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
