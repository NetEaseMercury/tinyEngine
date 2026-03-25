/**
 * @file VulkanRender_Pick.cpp
 * @brief Object-ID pick pass: R32_UINT color, separate render pass, readback buffer.
 */
#include "VulkanRender.hpp"
#include "vectex.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
	std::string JoinShaderDir(const std::string& vertexShaderPath, const char* pickFile)
	{
		const auto p = vertexShaderPath.find_last_of("/\\");
		if (p == std::string::npos) {
			return std::string(pickFile);
		}
		return vertexShaderPath.substr(0, p + 1) + pickFile;
	}

	constexpr uint32_t kPickPushConstantSize = 68u;
}

/** @brief Free pick FB, images, pipeline, readback mapping */
void VulkanRender::destroyPickPassResources()
{
	if (pickReadbackBufferMemory != VK_NULL_HANDLE && pickReadbackMapped != nullptr) {
		vkUnmapMemory(device, pickReadbackBufferMemory);
		pickReadbackMapped = nullptr;
	}
	if (pickReadbackBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, pickReadbackBuffer, nullptr);
		pickReadbackBuffer = VK_NULL_HANDLE;
	}
	if (pickReadbackBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, pickReadbackBufferMemory, nullptr);
		pickReadbackBufferMemory = VK_NULL_HANDLE;
	}
	if (pickCommandBuffer != VK_NULL_HANDLE && commandPool != VK_NULL_HANDLE) {
		vkFreeCommandBuffers(device, commandPool, 1, &pickCommandBuffer);
		pickCommandBuffer = VK_NULL_HANDLE;
	}
	if (pickFramebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(device, pickFramebuffer, nullptr);
		pickFramebuffer = VK_NULL_HANDLE;
	}
	if (pickPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pickPipeline, nullptr);
		pickPipeline = VK_NULL_HANDLE;
	}
	if (pickPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, pickPipelineLayout, nullptr);
		pickPipelineLayout = VK_NULL_HANDLE;
	}
	if (pickRenderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(device, pickRenderPass, nullptr);
		pickRenderPass = VK_NULL_HANDLE;
	}
	if (pickColorImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device, pickColorImageView, nullptr);
		pickColorImageView = VK_NULL_HANDLE;
	}
	if (pickColorImage != VK_NULL_HANDLE) {
		vkDestroyImage(device, pickColorImage, nullptr);
		pickColorImage = VK_NULL_HANDLE;
	}
	if (pickColorImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, pickColorImageMemory, nullptr);
		pickColorImageMemory = VK_NULL_HANDLE;
	}
	if (pickDepthImageView != VK_NULL_HANDLE) {
		vkDestroyImageView(device, pickDepthImageView, nullptr);
		pickDepthImageView = VK_NULL_HANDLE;
	}
	if (pickDepthImage != VK_NULL_HANDLE) {
		vkDestroyImage(device, pickDepthImage, nullptr);
		pickDepthImage = VK_NULL_HANDLE;
	}
	if (pickDepthImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, pickDepthImageMemory, nullptr);
		pickDepthImageMemory = VK_NULL_HANDLE;
	}
}

/** @brief Pick-sized color/depth, framebuffer, graphics pipeline, host-visible readback */
void VulkanRender::createPickPassResources()
{
	destroyPickPassResources();

	const uint32_t w = swapChainExtent.width;
	const uint32_t h = swapChainExtent.height;
	if (w == 0 || h == 0 || swapChainImages.empty() || boxDescriptorSets.empty()) {
		return;
	}

	const VkFormat pickColorFormat = VK_FORMAT_R32_UINT;
	const VkFormat depthFormat = findDepthFormat();

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = pickColorFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = depthFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	VkAttachmentReference depthRef{};
	depthRef.attachment = 1;
	depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo rpInfo{};
	rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	rpInfo.pAttachments = attachments.data();
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;
	rpInfo.dependencyCount = 1;
	rpInfo.pDependencies = &dep;
	if (vkCreateRenderPass(device, &rpInfo, nullptr, &pickRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pick render pass!");
	}

	createImage(w, h, pickColorFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pickColorImage, pickColorImageMemory);
	pickColorImageView = createImageView(pickColorImage, pickColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

	createImage(w, h, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pickDepthImage, pickDepthImageMemory);
	pickDepthImageView = createImageView(pickDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	std::array<VkImageView, 2> fbViews = { pickColorImageView, pickDepthImageView };
	VkFramebufferCreateInfo fbInfo{};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.renderPass = pickRenderPass;
	fbInfo.attachmentCount = static_cast<uint32_t>(fbViews.size());
	fbInfo.pAttachments = fbViews.data();
	fbInfo.width = w;
	fbInfo.height = h;
	fbInfo.layers = 1;
	if (vkCreateFramebuffer(device, &fbInfo, nullptr, &pickFramebuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pick framebuffer!");
	}

	const std::string pickVert = JoinShaderDir(imGUI->vertexShaderPath, "pick_vert.spv");
	const std::string pickFrag = JoinShaderDir(imGUI->vertexShaderPath, "pick_frag.spv");
	auto vertCode = readFile(pickVert);
	auto fragCode = readFile(pickFrag);
	if (vertCode.empty() || fragCode.empty()) {
		throw std::runtime_error("Failed to read pick_vert.spv / pick_frag.spv (check res path).");
	}

	VkShaderModule vertMod = createShaderModule(vertCode);
	VkShaderModule fragMod = createShaderModule(fragCode);

	VkPipelineShaderStageCreateInfo vertStage{};
	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.module = vertMod;
	vertStage.pName = "main";
	VkPipelineShaderStageCreateInfo fragStage{};
	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.module = fragMod;
	fragStage.pName = "main";
	VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

	VkPipelineVertexInputStateCreateInfo vin{};
	vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	auto bindingDesc = Vertex::getBindingDescription();
	auto attrDesc = Vertex::getAttributeDescriptions();
	vin.vertexBindingDescriptionCount = 1;
	vin.pVertexBindingDescriptions = &bindingDesc;
	vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size());
	vin.pVertexAttributeDescriptions = attrDesc.data();

	VkPipelineInputAssemblyStateCreateInfo ia{};
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport{};
	viewport.width = static_cast<float>(w);
	viewport.height = static_cast<float>(h);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = { w, h };
	VkPipelineViewportStateCreateInfo vp{};
	vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vp.viewportCount = 1;
	vp.pViewports = &viewport;
	vp.scissorCount = 1;
	vp.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rs{};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.lineWidth = 1.0f;
	// 与主场景一致时背面剔除会导致与部分网格绕序不一致；Pick 关闭剔除以保证盒子等可拾取。
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	VkPipelineMultisampleStateCreateInfo ms{};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo ds{};
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_LESS;

	VkPipelineColorBlendAttachmentState cba{};
	cba.blendEnable = VK_FALSE;
	cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
	VkPipelineColorBlendStateCreateInfo cb{};
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;

	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	pcRange.offset = 0;
	pcRange.size = kPickPushConstantSize;

	VkPipelineLayoutCreateInfo plInfo{};
	plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plInfo.setLayoutCount = 1;
	plInfo.pSetLayouts = &boxDescriptorSetLayout;
	plInfo.pushConstantRangeCount = 1;
	plInfo.pPushConstantRanges = &pcRange;
	if (vkCreatePipelineLayout(device, &plInfo, nullptr, &pickPipelineLayout) != VK_SUCCESS) {
		vkDestroyShaderModule(device, fragMod, nullptr);
		vkDestroyShaderModule(device, vertMod, nullptr);
		throw std::runtime_error("Failed to create pick pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo gp{};
	gp.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gp.stageCount = 2;
	gp.pStages = stages;
	gp.pVertexInputState = &vin;
	gp.pInputAssemblyState = &ia;
	gp.pViewportState = &vp;
	gp.pRasterizationState = &rs;
	gp.pMultisampleState = &ms;
	gp.pDepthStencilState = &ds;
	gp.pColorBlendState = &cb;
	gp.layout = pickPipelineLayout;
	gp.renderPass = pickRenderPass;
	gp.subpass = 0;

	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &pickPipeline) != VK_SUCCESS) {
		vkDestroyShaderModule(device, fragMod, nullptr);
		vkDestroyShaderModule(device, vertMod, nullptr);
		throw std::runtime_error("Failed to create pick pipeline!");
	}

	vkDestroyShaderModule(device, fragMod, nullptr);
	vkDestroyShaderModule(device, vertMod, nullptr);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = commandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	if (vkAllocateCommandBuffers(device, &allocInfo, &pickCommandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate pick command buffer!");
	}

	VkBufferCreateInfo bufInfo{};
	bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufInfo.size = sizeof(uint32_t);
	bufInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (vkCreateBuffer(device, &bufInfo, nullptr, &pickReadbackBuffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create pick readback buffer!");
	}

	VkMemoryRequirements req{};
	vkGetBufferMemoryRequirements(device, pickReadbackBuffer, &req);
	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = req.size;
	memAlloc.memoryTypeIndex = findMemoryType(req.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (vkAllocateMemory(device, &memAlloc, nullptr, &pickReadbackBufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate pick readback memory!");
	}
	vkBindBufferMemory(device, pickReadbackBuffer, pickReadbackBufferMemory, 0);
	if (vkMapMemory(device, pickReadbackBufferMemory, 0, sizeof(uint32_t), 0, &pickReadbackMapped) != VK_SUCCESS) {
		throw std::runtime_error("Failed to map pick readback buffer!");
	}
}

/** @brief Record offscreen pick, blit/read UINT32 id at (pixelX,pixelY) into outObjectId */
void VulkanRender::runPickPassReadId(uint32_t pixelX, uint32_t pixelY, uint32_t& outObjectId)
{
	outObjectId = kPickIdNone;
	if (pickFramebuffer == VK_NULL_HANDLE || pickCommandBuffer == VK_NULL_HANDLE || pickReadbackMapped == nullptr) {
		return;
	}
	if (vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE) {
		return;
	}
	if (swapChainImages.empty() || boxDescriptorSets.empty()) {
		return;
	}

	updateUniformBuffer(0);
	vkDeviceWaitIdle(device);

	VkCommandBuffer cb = pickCommandBuffer;
	vkResetCommandBuffer(cb, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("pick: begin command buffer");
	}

	VkRenderPassBeginInfo rpBegin{};
	rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBegin.renderPass = pickRenderPass;
	rpBegin.framebuffer = pickFramebuffer;
	rpBegin.renderArea.offset = { 0, 0 };
	rpBegin.renderArea.extent = swapChainExtent;
	VkClearValue clears[2]{};
	clears[0].color.uint32[0] = 0;
	clears[0].color.uint32[1] = 0;
	clears[0].color.uint32[2] = 0;
	clears[0].color.uint32[3] = 0;
	clears[1].depthStencil = { 1.0f, 0 };
	rpBegin.clearValueCount = 2;
	rpBegin.pClearValues = clears;

	vkCmdBeginRenderPass(cb, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pickPipeline);

	const VkBuffer vb = vertexBuffer;
	const VkDeviceSize off0 = 0;
	vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off0);
	vkCmdBindIndexBuffer(cb, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pickPipelineLayout, 0, 1,
		&boxDescriptorSets[0], 0, nullptr);

	if (modelIndexCount > 0) {
		const glm::mat4 mainM = glm::translate(glm::mat4(1.0f), mainModelPosition);
		std::array<uint8_t, kPickPushConstantSize> bytes{};
		std::memcpy(bytes.data(), &mainM, sizeof(glm::mat4));
		const uint32_t oid = kPickIdMainModel;
		std::memcpy(bytes.data() + sizeof(glm::mat4), &oid, sizeof(uint32_t));
		vkCmdPushConstants(cb, pickPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			kPickPushConstantSize, bytes.data());
		vkCmdDrawIndexed(cb, modelIndexCount, 1, 0, 0, 0);
	}

	const glm::mat4 identity(1.0f);
	for (size_t bi = 0; bi < boxIndexRanges.size(); ++bi) {
		const auto& range = boxIndexRanges[bi];
		std::array<uint8_t, kPickPushConstantSize> bytes{};
		std::memcpy(bytes.data(), &identity, sizeof(glm::mat4));
		const uint32_t oid = kPickIdBoxBase + static_cast<uint32_t>(bi);
		std::memcpy(bytes.data() + sizeof(glm::mat4), &oid, sizeof(uint32_t));
		vkCmdPushConstants(cb, pickPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
			kPickPushConstantSize, bytes.data());
		vkCmdDrawIndexed(cb, range.indexCount, 1, range.firstIndex, 0, 0);
	}

	vkCmdEndRenderPass(cb);

	VkImageMemoryBarrier toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	toTransfer.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toTransfer.image = pickColorImage;
	toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	toTransfer.subresourceRange.levelCount = 1;
	toTransfer.subresourceRange.layerCount = 1;
	toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
		nullptr, 0, nullptr, 1, &toTransfer);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { static_cast<int32_t>(pixelX), static_cast<int32_t>(pixelY), 0 };
	region.imageExtent = { 1, 1, 1 };
	vkCmdCopyImageToBuffer(cb, pickColorImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, pickReadbackBuffer, 1,
		&region);

	VkImageMemoryBarrier back{};
	back.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	back.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	back.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	back.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	back.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	back.image = pickColorImage;
	back.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	back.subresourceRange.levelCount = 1;
	back.subresourceRange.layerCount = 1;
	back.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	back.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
		nullptr, 0, nullptr, 1, &back);

	if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
		throw std::runtime_error("pick: end command buffer");
	}

	VkSubmitInfo sub{};
	sub.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	sub.commandBufferCount = 1;
	sub.pCommandBuffers = &cb;
	if (vkQueueSubmit(graphicsQueue, 1, &sub, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("pick: submit");
	}
	vkQueueWaitIdle(graphicsQueue);

	std::memcpy(&outObjectId, pickReadbackMapped, sizeof(uint32_t));
}
