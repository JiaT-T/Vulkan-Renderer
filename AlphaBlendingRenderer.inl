#pragma once
// Included inside RenderFeatureRenderer; contains alpha-texture, blend-state, and command methods.

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

