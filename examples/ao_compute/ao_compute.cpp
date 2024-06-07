/*
* Vulkan Example - ambient occlusion example
*
* Copyright (C) by Ivanov Ivan for diploma based on SachaWillems samples
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"
#include "VulkanglTFModel.h"
#include <array>
#include "../config.hpp"

#define SSAO_RADIUS 0.3f

template<std::size_t N, typename T>
constexpr std::array<T, N> array_repeat(const T& value) {
    std::array<T, N> ret;
    ret.fill(value);
    return ret;
}

class VulkanExample : public VulkanExampleBase
{
public:
	vkglTF::Model scene;

	struct {
		VkQueue queue{ VK_NULL_HANDLE };
		VkCommandPool commandPool{ VK_NULL_HANDLE };
		VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };
		VkSemaphore semaphore{ VK_NULL_HANDLE };  // sync with graphic

		enum {
			SSAO,
			BLUR_HORIZONTAL,
			BLUR_VERTICAL,
			COUNT
		};

		std::array<VkPipeline, COUNT> pipelines = array_repeat<COUNT>(VkPipeline(VK_NULL_HANDLE));
		std::array<VkPipelineLayout, COUNT> pipelineLayouts = array_repeat<COUNT>(VkPipelineLayout(VK_NULL_HANDLE));
		std::array<VkDescriptorSet, COUNT> descriptorSets = array_repeat<COUNT>(VkDescriptorSet(VK_NULL_HANDLE));
		std::array<VkDescriptorSetLayout, COUNT> descriptorSetLayouts = array_repeat<COUNT>(VkDescriptorSetLayout(VK_NULL_HANDLE));

		vks::Texture2D ssao;
		vks::Texture2D blurHorizontal;
		vks::Texture2D blurVertical;
	} compute;

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
		VkSemaphore semaphoreGbuf{ VK_NULL_HANDLE }; // sync with compute
		VkCommandBuffer offscreenCmdBuf { VK_NULL_HANDLE };

		enum {
			GBUFFER,
			COMPOSITION,
			COUNT
		};

		std::array<VkPipeline, COUNT> pipelines = array_repeat<COUNT>(VkPipeline(VK_NULL_HANDLE));
		std::array<VkPipelineLayout, COUNT> pipelineLayouts = array_repeat<COUNT>(VkPipelineLayout(VK_NULL_HANDLE));
		std::array<VkDescriptorSet, COUNT> descriptorSets = array_repeat<COUNT>(VkDescriptorSet(VK_NULL_HANDLE));
		std::array<VkDescriptorSetLayout, COUNT> descriptorSetLayouts = array_repeat<COUNT>(VkDescriptorSetLayout(VK_NULL_HANDLE));

		struct Offscreen : public FrameBuffer {
			enum {
				NORMAL,
				ALBEDO,
				DEPTH,
				COUNT
			};
			std::array<FrameBufferAttachment, COUNT> frameBufferAttachments;
		} offscreen;
	} graphic;

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
		vks::Buffer sceneParams;
		vks::Buffer ssaoParams;
		vks::Buffer blurParams;
	} uniformBuffers;

	// One sampler for the frame buffer color attachments
	VkSampler colorSampler;
	VkSampler linearSampler;

	VulkanExample() : VulkanExampleBase()
	{
		title = "Compute AO";
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
			vkDestroySemaphore(device, compute.semaphore, nullptr);
			vkDestroySemaphore(device, graphic.semaphoreGbuf, nullptr);	
			vkDestroyCommandPool(device, compute.commandPool, nullptr);

			vkDestroySampler(device, colorSampler, nullptr);
			vkDestroySampler(device, linearSampler, nullptr);

			// Attachments 
			
			auto &fba = graphic.offscreen.frameBufferAttachments;
			std::for_each(fba.begin(), fba.end(), [&device = device](auto &fb){ 
				fb.destroy(device);
			});

			graphic.offscreen.destroy(device);

			auto &graphicPpls = graphic.pipelines;
			std::for_each(graphicPpls.begin(), graphicPpls.end(), [&device = device](auto &ppl){ 
				vkDestroyPipeline(device, ppl, nullptr);
			});

			auto &graphicPplLayots = graphic.pipelineLayouts;
			std::for_each(graphicPplLayots.begin(), graphicPplLayots.end(), [&device = device](auto &pplLayout){ 
				vkDestroyPipelineLayout(device, pplLayout, nullptr);
			});

			auto &graphicDescSetLayouts = graphic.descriptorSetLayouts;
			std::for_each(graphicDescSetLayouts.begin(), graphicDescSetLayouts.end(), [&device = device](auto &descSetLayout){ 
				vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
			});

			auto &computePpls = compute.pipelines;
			std::for_each(computePpls.begin(), computePpls.end(), [&device = device](auto &ppl){ 
				vkDestroyPipeline(device, ppl, nullptr);
			});

			auto &computePplLayots = compute.pipelineLayouts;
			std::for_each(computePplLayots.begin(), computePplLayots.end(), [&device = device](auto &pplLayout){ 
				vkDestroyPipelineLayout(device, pplLayout, nullptr);
			});

			auto &computeDescSetLayouts = compute.descriptorSetLayouts;
			std::for_each(computeDescSetLayouts.begin(), computeDescSetLayouts.end(), [&device = device](auto &descSetLayout){ 
				vkDestroyDescriptorSetLayout(device, descSetLayout, nullptr);
			});

			compute.ssao.destroy();
			compute.blurHorizontal.destroy();
			compute.blurVertical.destroy();

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

	void createStorageImage(VkFormat format,
		VkImageUsageFlagBits usage,
		vks::Texture2D &attachment,
		uint32_t width,
		uint32_t height,
		bool onlyCompute = false)
	{
		attachment.width = width;
		attachment.height = height;
		VkFormatProperties formatProperties;
		// Get device properties for the requested texture format
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProperties);
		// Check if requested image format supports image storage operations required for storing pixel from the compute shader
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		VkImageCreateInfo imageCreateInfo = vks::initializers::imageCreateInfo();
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		// Image will be sampled in the fragment shader and used as storage target in the compute shader
		imageCreateInfo.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		imageCreateInfo.flags = 0;
		// If compute and graphics queue family indices differ, we create an image that can be shared between them
		// This can result in worse performance than exclusive sharing mode, but save some synchronization to keep the sample simple
		std::vector<uint32_t> queueFamilyIndices;
		if (!onlyCompute && (vulkanDevice->queueFamilyIndices.graphics != vulkanDevice->queueFamilyIndices.compute)) {
			queueFamilyIndices = {
				vulkanDevice->queueFamilyIndices.graphics,
				vulkanDevice->queueFamilyIndices.compute
			};
			imageCreateInfo.sharingMode = VK_SHARING_MODE_CONCURRENT;
			imageCreateInfo.queueFamilyIndexCount = 2;
			imageCreateInfo.pQueueFamilyIndices = queueFamilyIndices.data();
		}
		else {
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.queueFamilyIndexCount = 1;
			imageCreateInfo.pQueueFamilyIndices = &vulkanDevice->queueFamilyIndices.compute;
		}

		VK_CHECK_RESULT(vkCreateImage(device, &imageCreateInfo, nullptr, &attachment.image));

		VkMemoryAllocateInfo memAllocInfo = vks::initializers::memoryAllocateInfo();
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, attachment.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK_RESULT(vkAllocateMemory(device, &memAllocInfo, nullptr, &attachment.deviceMemory));
		VK_CHECK_RESULT(vkBindImageMemory(device, attachment.image, attachment.deviceMemory, 0));

		// Transition image to the general layout, so we can use it as a storage image in the compute shader
		VkCommandBuffer layoutCmd = vulkanDevice->createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		attachment.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vks::tools::setImageLayout(layoutCmd, attachment.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, attachment.imageLayout);
		vulkanDevice->flushCommandBuffer(layoutCmd, queue, true);

		// Create image view
		VkImageViewCreateInfo view = vks::initializers::imageViewCreateInfo();
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = format;
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.image = attachment.image;
		VK_CHECK_RESULT(vkCreateImageView(device, &view, nullptr, &attachment.view));

		// Initialize a descriptor for later use
		attachment.descriptor.imageLayout = attachment.imageLayout;
		attachment.descriptor.imageView = attachment.view;
		attachment.sampler = VK_NULL_HANDLE;
		attachment.device = vulkanDevice;
	}

	void prepareCompute() {
		const uint32_t ssaoWidth = width / 2;
		const uint32_t ssaoHeight = height / 2;

		VkFormat format = VK_FORMAT_R8_UNORM;

		createStorageImage(format, VK_IMAGE_USAGE_SAMPLED_BIT, compute.ssao, ssaoWidth, ssaoHeight);
		createStorageImage(format, VK_IMAGE_USAGE_SAMPLED_BIT, compute.blurHorizontal, ssaoWidth, ssaoHeight, true);
		createStorageImage(format, VK_IMAGE_USAGE_SAMPLED_BIT, compute.blurVertical, ssaoWidth, ssaoHeight);

		// Get a compute queue from the device
		vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.compute, 0, &compute.queue);
		
		// Separate command pool as queue family for compute may be different than graphics
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = vulkanDevice->queueFamilyIndices.compute;
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &compute.commandPool));

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo( compute.commandPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &compute.commandBuffer));
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
			((usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT  : 0);

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

		graphic.offscreen.setSize(width, height);

		// Find a suitable depth format
		VkFormat attDepthFormat;
		VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &attDepthFormat);
		assert(validDepthFormat);
		// G-Buffer
		
		createAttachment(VK_FORMAT_R16G16_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, 
			&graphic.offscreen.frameBufferAttachments[graphic.offscreen.NORMAL], width, height);
		createAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			&graphic.offscreen.frameBufferAttachments[graphic.offscreen.ALBEDO], width, height);			// Albedo (color)
		createAttachment(attDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
			&graphic.offscreen.frameBufferAttachments[graphic.offscreen.DEPTH], width, height);			// Depth

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
			attachmentDescs[0].format = graphic.offscreen.frameBufferAttachments[graphic.offscreen.NORMAL].format;
			attachmentDescs[1].format = graphic.offscreen.frameBufferAttachments[graphic.offscreen.ALBEDO].format;
			attachmentDescs[2].format = graphic.offscreen.frameBufferAttachments[graphic.offscreen.DEPTH].format;

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
			dependencies[1].dstStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
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
			VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &graphic.offscreen.renderPass));

			std::array<VkImageView, 3> attachments;
			attachments[0] = graphic.offscreen.frameBufferAttachments[graphic.offscreen.NORMAL].view;
			attachments[1] = graphic.offscreen.frameBufferAttachments[graphic.offscreen.ALBEDO].view;
			attachments[2] = graphic.offscreen.frameBufferAttachments[graphic.offscreen.DEPTH].view;

			VkFramebufferCreateInfo fbufCreateInfo = vks::initializers::framebufferCreateInfo();
			fbufCreateInfo.renderPass = graphic.offscreen.renderPass;
			fbufCreateInfo.pAttachments = attachments.data();
			fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			fbufCreateInfo.width = graphic.offscreen.width;
			fbufCreateInfo.height = graphic.offscreen.height;
			fbufCreateInfo.layers = 1;
			VK_CHECK_RESULT(vkCreateFramebuffer(device, &fbufCreateInfo, nullptr, &graphic.offscreen.frameBuffer));
		}

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

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo( cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
		VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &graphic.offscreenCmdBuf));
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

		/*
			Offscreen GBuffer generation
		*/
		{
			vkBeginCommandBuffer(graphic.offscreenCmdBuf, &cmdBufInfo);
			// Clear values for all attachments written in the fragment shader
			std::vector<VkClearValue> clearValues(3);
			clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
			clearValues[2].depthStencil = { 1.0f, 0 };

			VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
			renderPassBeginInfo.renderPass = graphic.offscreen.renderPass;
			renderPassBeginInfo.framebuffer = graphic.offscreen.frameBuffer;
			renderPassBeginInfo.renderArea.extent.width = graphic.offscreen.width;
			renderPassBeginInfo.renderArea.extent.height = graphic.offscreen.height;
			renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassBeginInfo.pClearValues = clearValues.data();

			/*
				First pass: Fill G-Buffer components (normals, albedo) using MRT
			*/

			vkCmdBeginRenderPass(graphic.offscreenCmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport = vks::initializers::viewport((float)graphic.offscreen.width, (float)graphic.offscreen.height, 0.0f, 1.0f);
			vkCmdSetViewport(graphic.offscreenCmdBuf, 0, 1, &viewport);

			VkRect2D scissor = vks::initializers::rect2D(graphic.offscreen.width, graphic.offscreen.height, 0, 0);
			vkCmdSetScissor(graphic.offscreenCmdBuf, 0, 1, &scissor);

			vkCmdBindPipeline(graphic.offscreenCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphic.pipelines[graphic.GBUFFER]);

			vkCmdBindDescriptorSets(graphic.offscreenCmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, graphic.pipelineLayouts[graphic.GBUFFER], 0, 1, &graphic.descriptorSets[graphic.GBUFFER], 0, nullptr);
			scene.draw(graphic.offscreenCmdBuf, vkglTF::RenderFlags::BindImages, graphic.pipelineLayouts[graphic.GBUFFER]);

			vkCmdEndRenderPass(graphic.offscreenCmdBuf);

			VK_CHECK_RESULT(vkEndCommandBuffer(graphic.offscreenCmdBuf));
		}

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

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

				vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphic.pipelineLayouts[graphic.COMPOSITION], 0, 1, &graphic.descriptorSets[graphic.COMPOSITION], 0, NULL);

				// Final composition pass
				vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphic.pipelines[graphic.COMPOSITION]);
				vkCmdDraw(drawCmdBuffers[i], 3, 1, 0, 0);

				drawUI(drawCmdBuffers[i]);

				vkCmdEndRenderPass(drawCmdBuffers[i]);
			}

			VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
		}

		{
			VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

			VK_CHECK_RESULT(vkBeginCommandBuffer(compute.commandBuffer, &cmdBufInfo));

			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.SSAO]);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayouts[compute.SSAO], 0, 1, &compute.descriptorSets[compute.SSAO], 0, 0);

			VkImageMemoryBarrier barrier{};
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

			std::vector<VkImageMemoryBarrier> barriers(2, barrier);
			barriers[0].image = compute.ssao.image;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

			barriers[1].image = compute.blurVertical.image;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[1].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barriers[1].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

			vkCmdPipelineBarrier(compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				2, barriers.data()
			);

			vkCmdDispatch(compute.commandBuffer, compute.ssao.width / 8, compute.ssao.height / 8, 1);

			barriers[0].image = compute.blurHorizontal.image;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

			barriers[1].image = compute.ssao.image;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			vkCmdPipelineBarrier(compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				2, barriers.data()
			);

			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.BLUR_HORIZONTAL]);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayouts[compute.BLUR_HORIZONTAL], 0, 1, &compute.descriptorSets[compute.BLUR_HORIZONTAL], 0, 0);
			vkCmdDispatch(compute.commandBuffer, compute.ssao.width / 8, compute.ssao.height, 1);

			barriers[0].image = compute.blurHorizontal.image;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			vkCmdPipelineBarrier(compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				1, barriers.data()
			);

			vkCmdBindPipeline(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelines[compute.BLUR_VERTICAL]);
			vkCmdBindDescriptorSets(compute.commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, compute.pipelineLayouts[compute.BLUR_VERTICAL], 0, 1, &compute.descriptorSets[compute.BLUR_VERTICAL], 0, 0);
			vkCmdDispatch(compute.commandBuffer, compute.ssao.width, compute.ssao.height / 8, 1);

			barriers[0].image = compute.ssao.image;
			barriers[0].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barriers[0].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[0].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			barriers[1].image = compute.blurVertical.image;
			barriers[1].oldLayout = VK_IMAGE_LAYOUT_GENERAL;
			barriers[1].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barriers[1].srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
			barriers[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

			vkCmdPipelineBarrier(compute.commandBuffer,
				VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0,
				0, nullptr,
				0, nullptr,
				2, barriers.data()
			);

			vkEndCommandBuffer(compute.commandBuffer);
		}
	}

	void setupDescriptors()
	{
		// Pool
		std::vector<VkDescriptorPoolSize> poolSizes = {
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 11),
			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 6),
		};
		VkDescriptorPoolCreateInfo descriptorPoolInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, uint32_t(5));
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
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &graphic.descriptorSetLayouts[graphic.GBUFFER]));

		descriptorAllocInfo.pSetLayouts = &graphic.descriptorSetLayouts[graphic.GBUFFER];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &graphic.descriptorSets[graphic.GBUFFER]));
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.GBUFFER], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, &uniformBuffers.sceneParams.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);

		// ssao compute 
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 2),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),								// FS Lights UBO
		};

		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device,	&setLayoutCreateInfo, nullptr, &compute.descriptorSetLayouts[compute.SSAO]));

		descriptorAllocInfo.pSetLayouts = &compute.descriptorSetLayouts[compute.SSAO];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &compute.descriptorSets[compute.SSAO]));
		imageDescriptors = {
			vks::initializers::descriptorImageInfo(colorSampler, graphic.offscreen.frameBufferAttachments[graphic.offscreen.DEPTH].view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, graphic.offscreen.frameBufferAttachments[graphic.offscreen.NORMAL].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		};
		std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.SSAO], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[0]),
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.SSAO], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.SSAO], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2, &compute.ssao.descriptor),
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.SSAO], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &uniformBuffers.ssaoParams.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

		// blur horizontal compute 
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1)								// FS Lights UBO
		};

		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device,	&setLayoutCreateInfo, nullptr, &compute.descriptorSetLayouts[compute.BLUR_HORIZONTAL]));

		descriptorAllocInfo.pSetLayouts = &compute.descriptorSetLayouts[compute.BLUR_HORIZONTAL];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &compute.descriptorSets[compute.BLUR_HORIZONTAL]));
		computeWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.BLUR_HORIZONTAL], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &compute.ssao.descriptor),
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.BLUR_HORIZONTAL], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &compute.blurHorizontal.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

		// blur vertical compute 
		setLayoutBindings = {
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
			vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 1)								// FS Lights UBO
		};

		setLayoutCreateInfo = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device,	&setLayoutCreateInfo, nullptr, &compute.descriptorSetLayouts[compute.BLUR_VERTICAL]));

		descriptorAllocInfo.pSetLayouts = &compute.descriptorSetLayouts[compute.BLUR_VERTICAL];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &compute.descriptorSets[compute.BLUR_VERTICAL]));
		computeWriteDescriptorSets = {
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.BLUR_VERTICAL], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &compute.blurHorizontal.descriptor),
			vks::initializers::writeDescriptorSet(compute.descriptorSets[compute.BLUR_VERTICAL], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &compute.blurVertical.descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);

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
		VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &setLayoutCreateInfo, nullptr, &graphic.descriptorSetLayouts[graphic.COMPOSITION]));
		descriptorAllocInfo.pSetLayouts = &graphic.descriptorSetLayouts[graphic.COMPOSITION];
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorAllocInfo, &graphic.descriptorSets[graphic.COMPOSITION]));
		imageDescriptors = {
			vks::initializers::descriptorImageInfo(colorSampler, graphic.offscreen.frameBufferAttachments[graphic.offscreen.DEPTH].view, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, graphic.offscreen.frameBufferAttachments[graphic.offscreen.NORMAL].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(colorSampler, graphic.offscreen.frameBufferAttachments[graphic.offscreen.ALBEDO].view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(linearSampler, compute.ssao.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
			vks::initializers::descriptorImageInfo(linearSampler, compute.blurVertical.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL),
		};
		writeDescriptorSets = {
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0, &imageDescriptors[0]),			// FS Sampler Depth
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, &imageDescriptors[1]),			// FS Sampler Normals
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2, &imageDescriptors[2]),			// FS Sampler Albedo
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3, &imageDescriptors[3]),			// FS Sampler SSAO
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, &imageDescriptors[4]),			// FS Sampler SSAO blurred
			vks::initializers::writeDescriptorSet(graphic.descriptorSets[graphic.COMPOSITION], VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5, &uniformBuffers.ssaoParams.descriptor),	// FS SSAO Params UBO
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	void preparePipelines()
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &graphic.semaphoreGbuf));

		// Semaphore for compute & graphics sync
		semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &compute.semaphore));

		// Layouts
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo();

		const std::vector<VkDescriptorSetLayout> setLayouts = { graphic.descriptorSetLayouts[graphic.GBUFFER], vkglTF::descriptorSetLayoutImage };
		pipelineLayoutCreateInfo.pSetLayouts = setLayouts.data();
		pipelineLayoutCreateInfo.setLayoutCount = 2;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &graphic.pipelineLayouts[graphic.GBUFFER]));

		pipelineLayoutCreateInfo.pSetLayouts = &graphic.descriptorSetLayouts[graphic.COMPOSITION];
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &graphic.pipelineLayouts[graphic.COMPOSITION]));

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

		VkGraphicsPipelineCreateInfo pipelineCreateInfo = vks::initializers::pipelineCreateInfo( graphic.pipelineLayouts[graphic.COMPOSITION], renderPass, 0);
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

		// // Final composition pipeline
		shaderStages[0] = loadShader(getShadersPath() + "ao_compute/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "ao_compute/composition.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphic.pipelines[graphic.COMPOSITION]));

		// Fill G-Buffer pipeline
		// Vertex input state from glTF model loader
		pipelineCreateInfo.pVertexInputState = vkglTF::Vertex::getPipelineVertexInputState({ vkglTF::VertexComponent::Position, vkglTF::VertexComponent::UV, vkglTF::VertexComponent::Color, vkglTF::VertexComponent::Normal });
		pipelineCreateInfo.renderPass = graphic.offscreen.renderPass;
		pipelineCreateInfo.layout = graphic.pipelineLayouts[graphic.GBUFFER];
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
		shaderStages[0] = loadShader(getShadersPath() + "ao_compute/gbuffer.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getShadersPath() + "ao_compute/gbuffer.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &graphic.pipelines[graphic.GBUFFER]));

		// Create compute pipeline
		// Compute pipelines are created separate from graphics pipelines even if they use the same queue
		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&compute.descriptorSetLayouts[compute.SSAO], 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayouts[compute.SSAO]));

		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&compute.descriptorSetLayouts[compute.BLUR_HORIZONTAL], 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayouts[compute.BLUR_HORIZONTAL]));

		pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&compute.descriptorSetLayouts[compute.BLUR_VERTICAL], 1);
		VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &compute.pipelineLayouts[compute.BLUR_VERTICAL]));

		// Create compute shader pipelines
		VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(compute.pipelineLayouts[compute.SSAO], 0);

		computePipelineCreateInfo.stage = loadShader(getShadersPath() + "ao_compute/ssao_test.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &compute.pipelines[compute.SSAO]));

		computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(compute.pipelineLayouts[compute.BLUR_HORIZONTAL], 0);
		computePipelineCreateInfo.stage = loadShader(getShadersPath() + "ao_compute/blur_horizontal.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &compute.pipelines[compute.BLUR_HORIZONTAL]));

		computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(compute.pipelineLayouts[compute.BLUR_VERTICAL], 0);
		computePipelineCreateInfo.stage = loadShader(getShadersPath() + "ao_compute/blur_vertical.comp.spv", VK_SHADER_STAGE_COMPUTE_BIT);
		VK_CHECK_RESULT(vkCreateComputePipelines(device, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &compute.pipelines[compute.BLUR_VERTICAL]));
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
		prepareCompute();
		prepareUniformBuffers();
		setupDescriptors();
		preparePipelines();
		buildCommandBuffers();
		prepared = true;
	}

	void draw()
	{
		VulkanExampleBase::prepareFrame();

		// graphic.offscreenCmdBuf

		std::vector<VkPipelineStageFlags> graphicsWaitStageMasks = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT };
		std::vector<VkSemaphore> graphicsWaitSemaphores = { semaphores.presentComplete };
		std::vector<VkSemaphore> graphicsSignalSemaphores = { graphic.semaphoreGbuf };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &graphic.offscreenCmdBuf;
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores.data();
		submitInfo.pWaitDstStageMask = graphicsWaitStageMasks.data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores.data();
		VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		VkSubmitInfo computeSubmitInfo = vks::initializers::submitInfo();
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &compute.commandBuffer;
		computeSubmitInfo.waitSemaphoreCount = 1;
		computeSubmitInfo.pWaitSemaphores = &graphic.semaphoreGbuf;
		computeSubmitInfo.pWaitDstStageMask = &waitStageMask;
		computeSubmitInfo.signalSemaphoreCount = 1;
		computeSubmitInfo.pSignalSemaphores = &compute.semaphore;
		VK_CHECK_RESULT(vkQueueSubmit(compute.queue, 1, &computeSubmitInfo, VK_NULL_HANDLE));

		// Submit graphics commands
		graphicsWaitStageMasks = { VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT };
		graphicsWaitSemaphores = { compute.semaphore };
		graphicsSignalSemaphores = { semaphores.renderComplete };
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = graphicsWaitSemaphores.data();
		submitInfo.pWaitDstStageMask = graphicsWaitStageMasks.data();
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = graphicsSignalSemaphores.data();
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
		// std::cout << "rot and pos" << std::endl;
		// std::cout << camera.rotation.x << ", " << camera.rotation.y << ", " << camera.rotation.z << std::endl;
		// std::cout << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << std::endl;
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
