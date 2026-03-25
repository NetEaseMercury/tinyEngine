/**
 * @file VulkanRender_Descriptor.cpp
 * @brief Uniform buffers, descriptor pool/sets, per-frame UBO map/write.
 */
#include "VulkanRender.hpp"
#include "VulkanRender_Globals.hpp"
#include <array>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <vector>

/** @brief Host-visible UBO per swapchain image */
void VulkanRender::createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);

	uniformBuffers.resize(swapChainImages.size());
	uniformBuffersMemory.resize(swapChainImages.size());

	for (size_t i = 0; i < swapChainImages.size(); ++i) {
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffers[i], uniformBuffersMemory[i]);
	}
}

/** @brief Pool for UBO + combined image samplers (main + box sets) */
void VulkanRender::createDescriptorPool()
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[0].descriptorCount = 1000;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = 1000;

	VkDescriptorPoolCreateInfo poolCreateInfo{};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCreateInfo.pPoolSizes = poolSizes.data();
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapChainImages.size()) * 2;

	if (vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create descriptor pool!");
	}
}

/** @brief Main pipeline: binding0 UBO, binding1 albedo, binding2 normal */
void VulkanRender::createDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	allocateInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(swapChainImages.size());
	if (vkAllocateDescriptorSets(device, &allocateInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < swapChainImages.size(); ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		VkDescriptorImageInfo normalInfo{};
		normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		normalInfo.imageView = normalImageView;
		normalInfo.sampler = normalSampler;

		std::array<VkWriteDescriptorSet, 3> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[2].dstSet = descriptorSets[i];
		descriptorWrites[2].dstBinding = 2;
		descriptorWrites[2].dstArrayElement = 0;
		descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[2].descriptorCount = 1;
		descriptorWrites[2].pImageInfo = &normalInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0,
			nullptr);
	}
}

/** @brief Box pipeline: binding0 UBO only */
void VulkanRender::createBoxDescriptorSets()
{
	std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), boxDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = descriptorPool;
	allocateInfo.descriptorSetCount = static_cast<uint32_t>(swapChainImages.size());
	allocateInfo.pSetLayouts = layouts.data();

	boxDescriptorSets.resize(swapChainImages.size());
	if (vkAllocateDescriptorSets(device, &allocateInfo, boxDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("Failed to allocate box descriptor sets!");
	}

	for (size_t i = 0; i < swapChainImages.size(); ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = boxDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

/** @brief Map UBO image slice and write view/proj/tints */
void VulkanRender::updateUniformBuffer(uint32_t currentImage)
{
	UniformBufferObject ubo{};
	ubo.view = camera.GetViewMatrix();
	ubo.proj = glm::perspective(glm::radians(45.0f),
		swapChainExtent.width / static_cast<float>(swapChainExtent.height),
		0.1f, 500.0f);
	ubo.proj[1][1] *= -1;
	ubo.materialTint = glm::vec4(materialTintRgb, 1.0f);
	ubo.boxMaterialTint = glm::vec4(boxMaterialTintRgb, 1.0f);

	void* data = nullptr;
	vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
}
