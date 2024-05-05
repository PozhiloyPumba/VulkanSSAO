/*
* Vulkan Example - ambient occlusion example
*
* Copyright (C) by Ivanov Ivan for diploma based on SachaWillems samples
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include "../config.hpp"

#define SSAO_RADIUS 0.3f

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	struct UBOSceneParams {
		glm::mat4 projection;
		glm::mat4 model;
		glm::mat4 view;
		float nearPlane = 0.1f;
		float farPlane = 64.0f;
	} uboSceneParams;

	struct UBOSSAOParams {
		glm::mat4 invProjection;
		int32_t ssao = true;
		int32_t ssaoOnly = false;
		int32_t ssaoBlur = true;
	} uboSSAOParams;

	struct UBOBlurParams {
		int32_t depthCheck = false;
		float depthRange = 0.001f;
		float nearPlane = 0.1f;
		float farPlane = 64.0f;
		int32_t useLerpTrick = true;
	} uboBlurParams;

	struct {
		VkPipeline offscreen{ VK_NULL_HANDLE };
		VkPipeline composition{ VK_NULL_HANDLE };
		VkPipeline ssao{ VK_NULL_HANDLE };
		VkPipeline blurHorizontal{ VK_NULL_HANDLE };
		VkPipeline blurVertical{ VK_NULL_HANDLE };
	} pipelines;

	struct {
		VkPipelineLayout gBuffer{ VK_NULL_HANDLE };
		VkPipelineLayout ssao{ VK_NULL_HANDLE };
		VkPipelineLayout blurHorizontal{ VK_NULL_HANDLE };
		VkPipelineLayout blurVertical{ VK_NULL_HANDLE };
		VkPipelineLayout composition{ VK_NULL_HANDLE };
	} pipelineLayouts;

	struct {
		VkDescriptorSet gBuffer{ VK_NULL_HANDLE };
		VkDescriptorSet ssao{ VK_NULL_HANDLE };
		VkDescriptorSet blurHorizontal{ VK_NULL_HANDLE };
		VkDescriptorSet blurVertical{ VK_NULL_HANDLE };
		VkDescriptorSet composition{ VK_NULL_HANDLE };
		const uint32_t count = 5;
	} descriptorSets;

	struct {
		VkDescriptorSetLayout gBuffer{ VK_NULL_HANDLE };
		VkDescriptorSetLayout ssao{ VK_NULL_HANDLE };
		VkDescriptorSetLayout blurHorizontal{ VK_NULL_HANDLE };
		VkDescriptorSetLayout blurVertical{ VK_NULL_HANDLE };
		VkDescriptorSetLayout composition{ VK_NULL_HANDLE };
	} descriptorSetLayouts;

	struct {
		vks::Buffer sceneParams;
		vks::Buffer ssaoParams;
		vks::Buffer blurParams;
	} uniformBuffers;

	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
		void destroy(VkDevice device)
		{
			vkDestroyImage(device, image, nullptr);
			vkDestroyImageView(device, view, nullptr);
			vkFreeMemory(device, mem, nullptr);
		}
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		VkRenderPass renderPass;
		void setSize(int32_t w, int32_t h)
		{
			this->width = w;
			this->height = h;
		}
		void destroy(VkDevice device)
		{
			vkDestroyFramebuffer(device, frameBuffer, nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);
		}
	};

	struct {
		struct Offscreen : public FrameBuffer {
			FrameBufferAttachment normal, albedo, depth;
		} offscreen;
		struct SSAO : public FrameBuffer {
			FrameBufferAttachment color;
		} ssao, aoBlurVertical, aoBlurHorizontal;
	} frameBuffers{};

	// One sampler for the frame buffer color attachments
	VkSampler colorSampler;
	VkSampler linearSampler;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Gaussian blur AO";
		camera.type = Camera::CameraType::firstperson;
#ifndef __ANDROID__
		camera.rotationSpeed = 0.25f;
#endif
		camera.position = conf.position;
		camera.setMovementSpeed(5.0f);
		camera.setRotation(conf.rotation);
		camera.setPerspective(60.0f, (float)width / (float)height, uboSceneParams.nearPlane, uboSceneParams.farPlane);
	}

	~VulkanExample()
	{
		if (device) {
			vkDestroySampler(device, colorSampler, nullptr);
			vkDestroySampler(device, linearSampler, nullptr);

			// Attachments 
			frameBuffers.offscreen.normal.destroy(device);
			frameBuffers.offscreen.albedo.destroy(device);
			frameBuffers.offscreen.depth.destroy(device);
			frameBuffers.ssao.color.destroy(device);
			frameBuffers.aoBlurHorizontal.color.destroy(device);
			frameBuffers.aoBlurVertical.color.destroy(device);

			// Framebuffers
			frameBuffers.offscreen.destroy(device);
			frameBuffers.ssao.destroy(device);
			frameBuffers.aoBlurHorizontal.destroy(device);
			frameBuffers.aoBlurVertical.destroy(device);

			vkDestroyPipeline(device, pipelines.offscreen, nullptr);
			vkDestroyPipeline(device, pipelines.composition, nullptr);
			vkDestroyPipeline(device, pipelines.ssao, nullptr);
			vkDestroyPipeline(device, pipelines.blurHorizontal, nullptr);
			vkDestroyPipeline(device, pipelines.blurVertical, nullptr);

			vkDestroyPipelineLayout(device, pipelineLayouts.gBuffer, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.ssao, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.blurHorizontal, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.blurVertical, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayouts.composition, nullptr);

			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.gBuffer, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.ssao, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.blurHorizontal, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.blurVertical, nullptr);
			vkDestroyDescriptorSetLayout(device, descriptorSetLayouts.composition, nullptr);

			// Uniform buffers
			uniformBuffers.sceneParams.destroy();
			uniformBuffers.ssaoParams.destroy();
			uniformBuffers.blurParams.destroy();
		}
	}

	void getEnabledFeatures()
	{
		enabledFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	}

	// Create a frame buffer attachment
	void createAttachment(
		VkFormat format,
		VkImageUsageFlagBits usage,
		FrameBufferAttachment *attachment,
		uint32_t width,
		uint32_t height)
	{
		VkImageAspectFlags aspectMask = 0;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
				aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image = vks::initializers::imageCreateInfo();
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = format;
		image.extent.width = width;
		image.extent.height = height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT | 
			((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT: 0);

		VkMemoryAllocateInfo memAlloc = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;

		VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &attachment->image));
		vkGetImageMemoryRequirements(device, attachment->image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAlloc, nullptr, &attachment->mem));
		VK_CHECK_RESULT(vkBindImageMemory(device, attachment->image, attachment->mem, 0));

		VkImageViewCreateInfo imageView = vks::initializers::imageViewCreateInfo();
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = attachment->image;
		VK_CHECK_RESULT(vkCreateImageView(device, &imageView, nullptr, &attachment->view));
	}

	void prepareOffscreenFramebuffers()
	{
		// Attachments
		const uint32_t ssaoWidth = width / 2;
		const uint32_t ssaoHeight = height / 2;

		frameBuffers.offscreen.setSize(width, height);
		frameBuffers.ssao.setSize(ssaoWidth, ssaoHeight);
		frameBuffers.aoBlurHorizontal.setSize(ssaoWidth, ssaoHeight);
		frameBuffers.aoBlurVertical.setSize(ssaoWidth, ssaoHeight);

		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
		assert(validDepthFormat);

		// G-Buffer
		createAttachment(VK_FORMAT_R16G16_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.offscreen.normal, width, height);			// Normals
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.offscreen.albedo, width, height);			// Albedo (color)
		createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &frameBuffers.offscreen.depth, width, height);			// Depth

		// SSAO
		createAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.ssao.color, ssaoWidth, ssaoHeight);				// Color

		// SSAO blur
		createAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.aoBlurHorizontal.color, ssaoWidth, ssaoHeight);					// Color
		createAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &frameBuffers.aoBlurVertical.color, ssaoWidth, ssaoHeight);					// Color

		// Render passes

		// G-Buffer creation
		{
			std::array<VkAttachmentDescription, 3> attachmentDescs = {};

			// Init attachment properties
			for (uint32_t i = 0; i < static_cast<uint32_t>(attachmentDescs.size()); i++)
			{
				attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
				attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
				attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
				attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
				attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
				attachmentDescs[i].finalLayout = (i == 2) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}

			// Formats
			attachmentDescs[0].format = frameBuffers.offscreen.normal.format;
			attachmentDescs[1].format = frameBuffers.offscreen.albedo.format;
			attachmentDescs[2].format = frameBuffers.offscreen.depth.format;

			std::vector<VkAttachmentReference> colorReferences;
			colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

			VkAttachmentReference depthReference = {};
			depthReference.attachment = 2;
			depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = colorReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
			subpass.pDepthStencilAttachment = &depthReference;

			// Use subpass dependencies for attachment layout transitions
			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = attachmentDescs.data();
			renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.offscreen.renderPass));

			std::array<VkImageView, 3> attachments;
			attachments[0] = frameBuffers.offscreen.normal.view;
			attachments[1] = frameBuffers.offscreen.albedo.view;
			attachments[2] = frameBuffers.offscreen.depth.view;

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.offscreen.renderPass;
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbufCreateInfo.width = frameBuffers.offscreen.width;
			fbufCreateInfo.height = frameBuffers.offscreen.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.offscreen.frameBuffer));
		}

		// SSAO
		{
			VkAttachmentDescription attachmentDescription{};
			attachmentDescription.format = frameBuffers.ssao.color.format;
			attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = &colorReference;
			subpass.colorAttachmentCount = 1;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuffers.ssao.renderPass));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuffers.ssao.renderPass;
			fbufCreateInfo.pAttachments = &frameBuffers.ssao.color.view;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.width = frameBuffers.ssao.width;
			fbufCreateInfo.height = frameBuffers.ssao.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuffers.ssao.frameBuffer));
		}

		// AO Blur 
		auto createBlurRenderPass = [&] (auto &frameBuf) {
			VkAttachmentDescription attachmentDescription{};
			attachmentDescription.format = frameBuf.color.format;
			attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.pColorAttachments = &colorReference;
			subpass.colorAttachmentCount = 1;

			std::array<VkSubpassDependency, 2> dependencies;

			dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[0].dstSubpass = 0;
			dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			dependencies[1].srcSubpass = 0;
			dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
			dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
			dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.pAttachments = &attachmentDescription;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 2;
			renderPassInfo.pDependencies = dependencies.data();
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &frameBuf.renderPass));

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = frameBuf.renderPass;
			fbufCreateInfo.pAttachments = &frameBuf.color.view;
			fbufCreateInfo.attachmentCount = 1;
			fbufCreateInfo.width = frameBuffers.aoBlurHorizontal.width;
			fbufCreateInfo.height = frameBuffers.aoBlurHorizontal.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &frameBuf.frameBuffer));
		};

		createBlurRenderPass(frameBuffers.aoBlurHorizontal);
		createBlurRenderPass(frameBuffers.aoBlurVertical);

		// Shared sampler used for all color attachments
		VkSamplerCreateInfo sampler = vks::initializers::samplerCreateInfo();
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &colorSampler));

		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		VK_CHECK_RESULT(vkCreateSampler(device, &sampler, nullptr, &linearSampler));
	}

	void loadAssets()
	{
		vkglTF::descriptorBindingFlags  = vkglTF::DescriptorBindingFlags::ImageBaseColor;
		const uint32_t gltfLoadingFlags = vkglTF::FileLoadingFlags::FlipY | vkglTF::FileLoadingFlags::PreTransformVertices;
		scene.loadFromFile(getAssetPath() + conf.name, vulkanDevice, queue, gltfLoadingFlags);
	}

	void buildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

			/*
				Offscreen SSAO generation
			*/
			{\
				// Clear values for all attachments written in the fragment shader
				std::vector<VkClearValue> clearValues(3);
				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[2].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = frameBuffers.offscreen.renderPass;
				renderPassBeginInfo.framebuffer = frameBuffers.offscreen.frameBuffer;
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.offscreen.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.offscreen.height;
				renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
				renderPassBeginInfo.pClearValues = clearValues.data();

				/*
					First pass: Fill G-Buffer components (normals, albedo) using MRT
				*/

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)frameBuffers.offscreen.width, (float)frameBuffers.offscreen.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(frameBuffers.offscreen.width, frameBuffers.offscreen.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.offscreen);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets.gBuffer, 0, nullptr);
				scene.draw(drawCmdBuffers[i], vkglTF::RenderFlags::BindImages, pipelineLayouts.gBuffer);

				vkCmdEndRenderPass(drawCmdBuffers[i]);

				/*
					Second pass: SSAO generation
				*/

				clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
				clearValues[1].depthStencil = { 1.0f, 0 };

				renderPassBeginInfo.framebuffer = frameBuffers.ssao.frameBuffer;
				renderPassBeginInfo.renderPass = frameBuffers.ssao.renderPass;
				renderPassBeginInfo.renderArea.extent.width = frameBuffers.ssao.width;
				renderPassBeginInfo.renderArea.extent.height = frameBuffers.ssao.height;
				renderPassBeginInfo.clearValueCount = 2;
				renderPassBeginInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				viewport = vks::initializers::viewport((float)frameBuffers.ssao.width, (float)frameBuffers.ssao.height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
				scissor = vks::initializers::rect2D(frameBuffers.ssao.width, frameBuffers.ssao.height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.ssao, 0, 1, &descriptorSets.ssao, 0, nullptr);
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.ssao);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(drawCmdBuffers[i]);

				/*
					Third pass: AO blur
				*/

				auto writeBlurRenderPass = [&] (auto &frameBuf, auto &pplLayout, auto &ppl, auto &descSet) {
					renderPassBeginInfo.framebuffer = frameBuf.frameBuffer;
					renderPassBeginInfo.renderPass = frameBuf.renderPass;
					renderPassBeginInfo.renderArea.extent.width = frameBuf.width;
					renderPassBeginInfo.renderArea.extent.height = frameBuf.height;

					vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

					viewport = vks::initializers::viewport((float)frameBuf.width, (float)frameBuf.height, 0.0f, 1.0f);
					vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
					scissor = vks::initializers::rect2D(frameBuf.width, frameBuf.height, 0, 0);
					vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

					vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pplLayout, 0, 1, &descSet, 0, nullptr);
					vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, ppl);
					vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

					vkCmdEndRenderPass(drawCmdBuffers[i]);
				};
				writeBlurRenderPass(frameBuffers.aoBlurHorizontal, pipelineLayouts.blurHorizontal, pipelines.blurHorizontal, descriptorSets.blurHorizontal);
				writeBlurRenderPass(frameBuffers.aoBlurVertical, pipelineLayouts.blurVertical, pipelines.blurVertical, descriptorSets.blurVertical);
			}

			/*
				Note: Explicit synchronization is not required between the render pass, as this is done implicit via sub pass dependencies
			*/

			/*
				Final render pass: Scene rendering with applied radial blur
			*/
			{
				std::vector<VkClearValue> clearValues(2);
				clearValues[0].color = defaultClearColor;
				clearValues[1].depthStencil = { 1.0f, 0 };

				VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
				renderPassBeginInfo.renderPass = renderPass;
				renderPassBeginInfo.framebuffer = VulkanExampleBase::frameBuffers[i];
				renderPassBeginInfo.renderArea.extent.width = width;
				renderPassBeginInfo.renderArea.extent.height = height;
				renderPassBeginInfo.clearValueCount = 2;
				renderPassBeginInfo.pClearValues = clearValues.data();

				vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

				VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
				vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

				VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
				vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.composition, 0, 1, &descriptorSets.composition, 0, NULL);

				// Final composition pass
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.composition);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				drawUI(drawCmdBuffers[i]);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 11)
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes,  descriptorSets.count);
		VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo;
		VkDescriptorSetAllocateInfo descriptorAllocInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, nullptr, 1);
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		std::vector<VkDescriptorImageInfo> imageDescriptors;

		// Layouts and Sets

		// G-Buffer creation (offscreen scene rendering)
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0),	// VS + FS Parameter UBO
		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.gBuffer));

		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.gBuffer;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.gBuffer));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.gBuffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.sceneParams.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// SSAO Generation
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0), 						// FS Depth
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),						// FS Normals
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),								// FS Params UBO
		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.ssao));

		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.ssao;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.ssao));
		imageDescriptors = {
			vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.normal.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		};
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.ssao, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[0]),					// FS Depth
			vks::initializers::writeDescriptorSet(descriptorSets.ssao, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),					// FS Normals
			vks::initializers::writeDescriptorSet(descriptorSets.ssao, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffers.ssaoParams.descriptor),		// FS SSAO Params UBO
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// AO Blur
		auto updateBlurDescriptorSets = [&] (auto &descSet, auto &descSetLayout, auto &inputFrameBuf) {
			setLayoutBindings = {
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),						// FS Sampler SSAO
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),						// FS Sampler Depth		// FS Blur Params UBO
				vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),								// FS Blur Params UBO
			};
			setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descSetLayout));
			descriptorAllocInfo.pSetLayouts = &descSetLayout;
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descSet));
			imageDescriptors = {
				vks::initializers::descriptorImageInfo(linearSampler, inputFrameBuf.color.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
				vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
			};
			writeDescriptorSets = {
				vks::initializers::writeDescriptorSet(descSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[0]),
				vks::initializers::writeDescriptorSet(descSet, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),
				vks::initializers::writeDescriptorSet(descSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffers.blurParams.descriptor),
			};
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
		};
		updateBlurDescriptorSets(descriptorSets.blurHorizontal, descriptorSetLayouts.blurHorizontal, frameBuffers.ssao);
		updateBlurDescriptorSets(descriptorSets.blurVertical, descriptorSetLayouts.blurVertical, frameBuffers.aoBlurHorizontal);
		
		// Composition
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0),						// FS Depth
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1),						// FS Normals
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 2),						// FS Albedo
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 3),						// FS SSAO
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 4),						// FS SSAO blurred
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT, 5),								// FS Lights UBO
		};
		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &descriptorSetLayouts.composition));
		descriptorAllocInfo.pSetLayouts = &descriptorSetLayouts.composition;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSets.composition));
		imageDescriptors = {
			vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.depth.view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.normal.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, frameBuffers.offscreen.albedo.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(linearSampler, frameBuffers.ssao.color.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(linearSampler, frameBuffers.aoBlurVertical.color.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		};
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[0]),			// FS Sampler Depth
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),			// FS Sampler Normals
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescriptors[2]),			// FS Sampler Albedo
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescriptors[3]),			// FS Sampler SSAO
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &imageDescriptors[4]),			// FS Sampler SSAO blurred
			vks::initializers::writeDescriptorSet(descriptorSets.composition, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &uniformBuffers.ssaoParams.descriptor),	// FS SSAO Params UBO
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		// Layouts
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo();

		const std::vector<VkDescriptorSetLayout> setLayouts = { descriptorSetLayouts.gBuffer, vkglTF::descriptorSetLayoutImage };
		pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
		pipelineLayoutCreateInfo.setLayoutCount = 2;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.gBuffer));

		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.ssao;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.ssao));

		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.blurHorizontal;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.blurHorizontal));

		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.blurVertical;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.blurVertical));

		pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayouts.composition;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayouts.composition));

		// Pipelines
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = vks::initializers::pipelineInputAssemblyStateCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, 0, VK_FALSE);
		VkPipelineRasterizationStateCreateInfo rasterizationState = vks::initializers::pipelineRasterizationStateCreateInfo(VK_POLYGON_MODE_FILL, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE, 0);
		VkPipelineColorBlendAttachmentState blendAttachmentState = vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE);
		VkPipelineColorBlendStateCreateInfo colorBlendState = vks::initializers::pipelineColorBlendStateCreateInfo(1, &blendAttachmentState);
		VkPipelineDepthStencilStateCreateInfo depthStencilState = vks::initializers::pipelineDepthStencilStateCreateInfo(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
		VkPipelineViewportStateCreateInfo viewportState = vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);
		VkPipelineMultisampleStateCreateInfo multisampleState = vks::initializers::pipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT, 0);
		std::vector<VkDynamicState> dynamicStateEnables = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState = vks::initializers::pipelineDynamicStateCreateInfo(dynamicStateEnables);
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo( pipelineLayouts.composition, renderPass, 0);
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		// Empty vertex input state for fullscreen passes
		VkPipelineVertexInputStateCreateInfo emptyVertexInputState = vks::initializers::pipelineVertexInputStateCreateInfo();
		pipelineCreateInfo.pVertexInputState = &emptyVertexInputState;
		rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;

		// Final composition pipeline
		shaderStages[0] = loadShader(getShadersPath() + "ao_gaussian_blur/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "ao_gaussian_blur/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.composition));

		// SSAO generation pipeline
		pipelineCreateInfo.renderPass = frameBuffers.ssao.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.ssao;
		// SSAO Kernel size and radius are constant for this pipeline, so we set them using specialization constants
		struct SpecializationData {
			float radius = SSAO_RADIUS;
		} specializationData;
		std::array<VkSpecializationMapEntry, 1> specializationMapEntries = {
			vks::initializers::specializationMapEntry(0, offsetof(SpecializationData, radius), sizeof(SpecializationData::radius))
		};
		VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(1, specializationMapEntries.data(), sizeof(specializationData), &specializationData);
		shaderStages[1] = loadShader(getShadersPath() + "ao_gaussian_blur/ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		shaderStages[1].pSpecializationInfo = &specializationInfo;
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.ssao));

		// AO blur pipelines
		pipelineCreateInfo.renderPass = frameBuffers.aoBlurHorizontal.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.blurHorizontal;
		shaderStages[1] = loadShader(getShadersPath() + "ao_gaussian_blur/blur_horizontal.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.blurHorizontal));

		pipelineCreateInfo.renderPass = frameBuffers.aoBlurVertical.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.blurVertical;
		shaderStages[1] = loadShader(getShadersPath() + "ao_gaussian_blur/blur_vertical.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.blurVertical));

		// Fill G-Buffer pipeline
		// Vertex input state from glTF model loader
		pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
		pipelineCreateInfo.renderPass = frameBuffers.offscreen.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.gBuffer;
		// Blend attachment states required for all color attachments
		// This is important, as color write mask will otherwise be 0x0 and you
		// won't see anything rendered to the attachment
		std::array<VkPipelineColorBlendAttachmentState, 2> blendAttachmentStates = {
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE),
			vks::initializers::pipelineColorBlendAttachmentState(0xf, VK_FALSE)
		};
		colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		colorBlendState.pAttachments = blendAttachmentStates.data();
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		shaderStages[0] = loadShader(getShadersPath() + "ao_gaussian_blur/gbuffer.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "ao_gaussian_blur/gbuffer.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.offscreen));
	}

	float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers()
	{
		// Scene matrices
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.sceneParams,
			sizeof(uboSceneParams));

		// SSAO parameters
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.ssaoParams,
			sizeof(uboSSAOParams));

		// Blur parameters
		vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffers.blurParams,
			sizeof(uboBlurParams));

		// Update
		updateUniformBufferMatrices();
		updateUniformBufferSSAOParams();
		updateUniformBufferBlurParams();
	}

	void updateUniformBufferMatrices()
	{
		uboSceneParams.projection = camera.matrices.perspective;
		uboSceneParams.view = camera.matrices.view;
		uboSceneParams.model = glm::mat4(1.0f);

		VK_CHECK_RESULT(uniformBuffers.sceneParams.map());
		uniformBuffers.sceneParams.copyTo(&uboSceneParams, sizeof(uboSceneParams));
		uniformBuffers.sceneParams.unmap();
	}

	void updateUniformBufferSSAOParams()
	{
		uboSSAOParams.invProjection = glm::inverse(camera.matrices.perspective);

		VK_CHECK_RESULT(uniformBuffers.ssaoParams.map());
		uniformBuffers.ssaoParams.copyTo(&uboSSAOParams, sizeof(uboSSAOParams));
		uniformBuffers.ssaoParams.unmap();
	}

	void updateUniformBufferBlurParams()
	{
		VK_CHECK_RESULT(uniformBuffers.blurParams.map());
		uniformBuffers.blurParams.copyTo(&uboBlurParams, sizeof(uboBlurParams));
		uniformBuffers.blurParams.unmap();
	}


	void prepare()
	{
		VulkanExampleBase::prepare();
		loadAssets();
		prepareOffscreenFramebuffers();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
		VulkanExampleBase::submitFrame();
	}

	virtual void render()
	{
		if (!prepared) {
			return;
		}
		updateUniformBufferMatrices();
		updateUniformBufferSSAOParams();
		updateUniformBufferBlurParams();
		draw();
	}

	virtual void OnUpdateUIOverlay(vks::UIOverlay *overlay)
	{
		if (overlay->header("Settings")) {
			overlay->checkBox("Enable SSAO", &uboSSAOParams.ssao);
			overlay->checkBox("SSAO blur", &uboSSAOParams.ssaoBlur);
			overlay->checkBox("SSAO pass only", &uboSSAOParams.ssaoOnly);
			overlay->checkBox("Blur depth check", &uboBlurParams.depthCheck);
			overlay->checkBox("Use lerp in blur", &uboBlurParams.useLerpTrick);
			overlay->inputFloat("Blur depth range", &uboBlurParams.depthRange, 0.0001f, 10);
		}
	}
};

VULKAN_EXAMPLE_MAIN()
