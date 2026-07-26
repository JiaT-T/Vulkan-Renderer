#pragma once
// Included inside RenderFeatureRenderer; contains depth-buffer resource and command methods.

	VkResult CreateDepthImage()
	{
		depthFormat = FindDepthFormat();
		if (depthFormat == VK_FORMAT_UNDEFINED)
		{
			outStream << "[ DepthRendering ] ERROR\nNo sampled depth-attachment format is supported.\n";
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
			result = CreateDepthScenePipeline(depthSceneVertexShader, depthSceneFragmentShader,
				depthScenePipelineLayout, depthRenderPass, depthDisabledPipeline, windowSize, 1, false);
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

	void RecordDepthFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkClearValue depthClears[2] = {};
		depthClears[0].color = { { 0.02f, 0.025f, 0.04f, 1.0f } };
		depthClears[1].depthStencil = { 1.0f, 0 };
		BeginRenderPass(commandBuffer, depthRenderPass, depthFramebuffer, windowSize, depthClears);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			example == RenderExample::DepthDisabled ? VkPipeline(depthDisabledPipeline) : VkPipeline(depthScenePipeline));
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

