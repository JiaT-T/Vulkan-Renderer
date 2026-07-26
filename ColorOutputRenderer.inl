#pragma once
// Included inside RenderFeatureRenderer; contains sRGB, HDR, and tone-mapping methods.

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
			outStream << std::format("[ ColorOutput ] Surface candidate: format={} colorSpace={}\n",
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
				outStream << "[ ColorOutput ] HDR surface format is unavailable; using the SDR fallback.\n";
		}

		if (selected.format != current.imageFormat || selected.colorSpace != current.imageColorSpace)
		{
			if (VkResult result = graphicsBase::Base().SetSurfaceFormat(selected))
				return result;
		}
		outputIsSrgbAttachment = selected.format == VK_FORMAT_R8G8B8A8_SRGB ||
			selected.format == VK_FORMAT_B8G8R8A8_SRGB;
		outStream << std::format(
			"[ ColorOutput ] Selected output: format={} colorSpace={} hdrActive={} encoding={} sRGB-attachment={}\n",
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

