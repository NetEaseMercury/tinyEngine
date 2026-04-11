/**
 * @file VulkanRender_Command.cpp
 * @brief Command pool/buffers, main pass recording, swapchain present loop.
 */
#include "VulkanRender.hpp"
#include "TinyEngineDebug.hpp"
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <array>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

/** @brief Main command pool with RESET_COMMAND_BUFFER_BIT */
void VulkanRender::createCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool!");
	}
}

/** @brief One command buffer per swapchain framebuffer; initial record */
void VulkanRender::createCommandBuffers()
{
	commandBuffers.resize(swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = commandPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = (uint32_t)commandBuffers.size();

	if (vkAllocateCommandBuffers(device, &allocateInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command buffers!");
	}

	for (size_t i = 0; i < commandBuffers.size(); ++i) {
		recordCommandBuffer(static_cast<uint32_t>(i));
	}
}

/** @brief Record main render pass: scene mesh, boxes, ImGui draw */
void VulkanRender::recordCommandBuffer(uint32_t i)
{
	VkCommandBuffer cb = commandBuffers[i];
	vkResetCommandBuffer(cb, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(cb, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("Failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = swapChainFramebuffers[i];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = swapChainExtent;

	std::array<VkClearValue, 2> clearValues{};
	if (imGUI != nullptr) {
		const ImVec4 cc = imGUI->getClearColor();
		clearValues[0].color = { cc.x * cc.w, cc.y * cc.w, cc.z * cc.w, cc.w };
	}
	else {
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
	}
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	TINYENGINE(cb, "Main Render Pass");
	vkCmdBeginRenderPass(cb, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	{
		TINYENGINE(cb, "Scene Geometry");
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkBuffer vertexBuffers[] = { vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(cb, 0, 1, vertexBuffers, offsets);

		vkCmdBindIndexBuffer(cb, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
			&descriptorSets[i], 0, nullptr);

		const glm::mat4 modelMatrix = glm::translate(glm::mat4(1.0f), mainModelPosition);
		vkCmdPushConstants(cb, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &modelMatrix);

		vkCmdDrawIndexed(cb, modelIndexCount, 1, 0, 0, 0);
	}

	if (!boxIndexRanges.empty()) {
		TINYENGINE(cb, "Box Geometry");
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, boxGraphicsPipeline);
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, boxPipelineLayout, 0, 1,
			&boxDescriptorSets[i], 0, nullptr);
		const glm::mat4 identity(1.0f);
		vkCmdPushConstants(cb, boxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &identity);
		for (const auto& range : boxIndexRanges) {
			vkCmdDrawIndexed(cb, range.indexCount, 1, range.firstIndex, 0, 0);
		}
	}

	if (ImGui::GetCurrentContext() != nullptr) {
		ImDrawData* dd = ImGui::GetDrawData();
		if (dd != nullptr && dd->Valid) {
			TINYENGINE(cb, "ImGui Overlay");
			ImGui_ImplVulkan_RenderDrawData(dd, cb);
		}
	}

	vkCmdEndRenderPass(cb);
	if (vkEndCommandBuffer(cb) != VK_SUCCESS) {
		throw std::runtime_error("Failed to record command buffer");
	}
}

/** @brief Acquire, UBO update, submit graphics, present; handle OUT_OF_DATE */
void VulkanRender::drawFrame()
{
	vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

	uint32_t imageIndex = 0;

	VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
		VK_NULL_HANDLE, &imageIndex);
	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("Failed to acquire swap chain image!");
	}
	updateUniformBuffer(imageIndex);

	if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
		vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
	}
	recordCommandBuffer(imageIndex);
	imagesInFlight[imageIndex] = inFlightFences[currentFrame];
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;
	vkResetFences(device, 1, &inFlightFences[currentFrame]);

	if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
		throw std::runtime_error("Failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &imageIndex;

	presentInfo.pResults = nullptr;

	result = vkQueuePresentKHR(presentQueue, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
		framebufferResized = false;
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("Failed to present swapchain image!");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}
