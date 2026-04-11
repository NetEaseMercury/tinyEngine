/**
 * @file VulkanRender_Buffer.cpp
 * @brief GPU buffer helpers: allocation, staging upload, single-submit commands, extra command pool.
 */
#include "VulkanRender.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

/** @brief Create buffer and bind device memory matching usage/properties */
void VulkanRender::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create buffer");
	}

	VkMemoryRequirements memRequirements{};
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate buffer memory!");
	}

	vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

/** @brief Pick memory type index satisfying typeFilter bits and property flags */
uint32_t VulkanRender::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

/** @brief One-time command: vkCmdCopyBuffer src→dst */
void VulkanRender::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion{};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}

/** @brief Staging upload of merged vertices to DEVICE_LOCAL vertex buffer */
void VulkanRender::createVertexBuffer()
{
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer{};
	VkDeviceMemory stagingBufferMemory{};

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
		stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

	copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

/** @brief Staging upload of merged indices to DEVICE_LOCAL index buffer */
void VulkanRender::createIndexBuffer()
{
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer{};
	VkDeviceMemory stagingBufferMemory{};

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		stagingBuffer, stagingBufferMemory);

	void* data = nullptr;
	vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(device, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

	copyBuffer(stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);
}

/** @brief Allocate one primary command buffer and begin ONE_TIME_SUBMIT */
VkCommandBuffer VulkanRender::beginSingleTimeCommands()
{
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = commandPool;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

/** @brief Submit command buffer to graphics queue, wait idle, free buffer */
void VulkanRender::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(graphicsQueue);

	vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

/** @brief Command pool on graphics queue family (used for transient uploads) */
void VulkanRender::createVulkanCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create command pool!");
	}
}

/** @brief Upload cubeTemplateVertices to DEVICE_LOCAL vertex buffer and cubeTemplateIndices to index buffer */
void VulkanRender::createCubeGpuBuffers()
{
	// Vertex buffer
	{
		VkDeviceSize bufferSize = sizeof(cubeTemplateVertices[0]) * cubeTemplateVertices.size();

		VkBuffer stagingBuffer{};
		VkDeviceMemory stagingBufferMemory{};
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data = nullptr;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, cubeTemplateVertices.data(), static_cast<size_t>(bufferSize));
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cubeVertexBuffer, cubeVertexBufferMemory);
		copyBuffer(stagingBuffer, cubeVertexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	// Index buffer
	{
		VkDeviceSize bufferSize = sizeof(cubeTemplateIndices[0]) * cubeTemplateIndices.size();

		VkBuffer stagingBuffer{};
		VkDeviceMemory stagingBufferMemory{};
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data = nullptr;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, cubeTemplateIndices.data(), static_cast<size_t>(bufferSize));
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, cubeIndexBuffer, cubeIndexBufferMemory);
		copyBuffer(stagingBuffer, cubeIndexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	cubeIndexCount = static_cast<uint32_t>(cubeTemplateIndices.size());
}

/** @brief Destroy cube template VBO/IBO */
void VulkanRender::destroyCubeGpuBuffers()
{
	if (cubeVertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, cubeVertexBuffer, nullptr);
		cubeVertexBuffer = VK_NULL_HANDLE;
	}
	if (cubeVertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, cubeVertexBufferMemory, nullptr);
		cubeVertexBufferMemory = VK_NULL_HANDLE;
	}
	if (cubeIndexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, cubeIndexBuffer, nullptr);
		cubeIndexBuffer = VK_NULL_HANDLE;
	}
	if (cubeIndexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, cubeIndexBufferMemory, nullptr);
		cubeIndexBufferMemory = VK_NULL_HANDLE;
	}
	cubeIndexCount = 0;
}

/** @brief Rebuild HOST_VISIBLE instance buffer from sorted boxes map; updates boxRangeEntityIds */
void VulkanRender::rebuildInstanceBuffer()
{
	vkDeviceWaitIdle(device);
	destroyInstanceBuffer();

	boxRangeEntityIds.clear();

	std::vector<std::pair<RenderEntityId, glm::vec3>> boxList(boxes.begin(), boxes.end());
	std::sort(boxList.begin(), boxList.end(),
		[](const std::pair<RenderEntityId, glm::vec3>& a, const std::pair<RenderEntityId, glm::vec3>& b) {
			return a.first < b.first;
		});

	instanceCount = static_cast<uint32_t>(boxList.size());
	if (instanceCount == 0) {
		return;
	}

	std::vector<InstanceData> instances;
	instances.reserve(instanceCount);
	for (const auto& kv : boxList) {
		instances.push_back(InstanceData{ kv.second });
		boxRangeEntityIds.push_back(kv.first);
	}

	const VkDeviceSize bufferSize = sizeof(InstanceData) * instanceCount;
	createBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		instanceBuffer, instanceBufferMemory);

	void* data = nullptr;
	vkMapMemory(device, instanceBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, instances.data(), static_cast<size_t>(bufferSize));
	vkUnmapMemory(device, instanceBufferMemory);
}

/** @brief Destroy instance buffer and reset instanceCount */
void VulkanRender::destroyInstanceBuffer()
{
	if (instanceBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, instanceBuffer, nullptr);
		instanceBuffer = VK_NULL_HANDLE;
	}
	if (instanceBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, instanceBufferMemory, nullptr);
		instanceBufferMemory = VK_NULL_HANDLE;
	}
	instanceCount = 0;
}
