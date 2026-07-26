#pragma once
// Included inside RenderFeatureRenderer; contains G-buffer and deferred-rendering methods.

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

