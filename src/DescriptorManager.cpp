#include "DescriptorManager.hpp"
#include "VulkanTypes.hpp"
#include <array>
#include <cstring>
#include <stdexcept>

void DescriptorManager::create(const VulkanContext& ctx, const SwapChain& swapChain,
                                const PipelineManager& pipelineMgr, const TextureManager& textureMgr,
                                const BufferManager& bufMgr)
{
    const uint32_t count = swapChain.getImageCount();
    createUniformBuffers(ctx, count, bufMgr);
    createPool(ctx, count);
    createMainSets(ctx, pipelineMgr, textureMgr);
    createBoxSets(ctx, pipelineMgr);
}

void DescriptorManager::recreate(const VulkanContext& ctx, const SwapChain& swapChain,
                                  const PipelineManager& pipelineMgr, const TextureManager& textureMgr,
                                  const BufferManager& bufMgr)
{
    destroy(ctx);
    create(ctx, swapChain, pipelineMgr, textureMgr, bufMgr);
}

void DescriptorManager::destroy(const VulkanContext& ctx)
{
    auto dev = ctx.getDevice();
    for (size_t i = 0; i < ubos_.size(); ++i) {
        if (uboMapped_[i]) vkUnmapMemory(dev, uboMemory_[i]);
        if (ubos_[i]      != VK_NULL_HANDLE) vkDestroyBuffer(dev, ubos_[i], nullptr);
        if (uboMemory_[i] != VK_NULL_HANDLE) vkFreeMemory(dev, uboMemory_[i], nullptr);
    }
    ubos_.clear(); uboMemory_.clear(); uboMapped_.clear();
    if (pool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(dev, pool_, nullptr);
        pool_ = VK_NULL_HANDLE;
    }
    mainDescSets_.clear();
    boxDescSets_.clear();
}

void DescriptorManager::updateUniformBuffer(uint32_t frameIndex,
                                             const glm::mat4& view, const glm::mat4& proj,
                                             const glm::vec3& materialTint,
                                             const glm::vec3& boxMaterialTint)
{
    UniformBufferObject ubo{};
    ubo.view           = view;
    ubo.proj           = proj;
    ubo.materialTint   = glm::vec4(materialTint,    1.0f);
    ubo.boxMaterialTint = glm::vec4(boxMaterialTint, 1.0f);
    memcpy(uboMapped_[frameIndex], &ubo, sizeof(ubo));
}

void DescriptorManager::createUniformBuffers(const VulkanContext& ctx, uint32_t count,
                                              const BufferManager& bufMgr)
{
    ubos_.resize(count);
    uboMemory_.resize(count);
    uboMapped_.resize(count, nullptr);

    for (uint32_t i = 0; i < count; ++i) {
        bufMgr.createBuffer(sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            ubos_[i], uboMemory_[i]);
        vkMapMemory(ctx.getDevice(), uboMemory_[i], 0, sizeof(UniformBufferObject), 0, &uboMapped_[i]);
    }
}

void DescriptorManager::createPool(const VulkanContext& ctx, uint32_t imageCount)
{
    std::array<VkDescriptorPoolSize, 2> sizes{};
    sizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sizes[0].descriptorCount = 1000;
    sizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sizes[1].descriptorCount = 1000;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = static_cast<uint32_t>(sizes.size());
    ci.pPoolSizes    = sizes.data();
    ci.maxSets       = imageCount * 2;
    if (vkCreateDescriptorPool(ctx.getDevice(), &ci, nullptr, &pool_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool!");
}

void DescriptorManager::createMainSets(const VulkanContext& ctx, const PipelineManager& pipelineMgr,
                                        const TextureManager& textureMgr)
{
    const auto count = static_cast<uint32_t>(ubos_.size());
    std::vector<VkDescriptorSetLayout> layouts(count, pipelineMgr.getMainDescSetLayout());

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = pool_;
    ai.descriptorSetCount = count;
    ai.pSetLayouts        = layouts.data();

    mainDescSets_.resize(count);
    if (vkAllocateDescriptorSets(ctx.getDevice(), &ai, mainDescSets_.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate main descriptor sets!");

    for (uint32_t i = 0; i < count; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = ubos_[i]; bi.offset = 0; bi.range = sizeof(UniformBufferObject);

        VkDescriptorImageInfo texInfo{};
        texInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        texInfo.imageView   = textureMgr.getTextureImageView();
        texInfo.sampler     = textureMgr.getTextureSampler();

        VkDescriptorImageInfo nrmInfo{};
        nrmInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        nrmInfo.imageView   = textureMgr.getNormalImageView();
        nrmInfo.sampler     = textureMgr.getNormalSampler();

        std::array<VkWriteDescriptorSet, 3> writes{};
        writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet          = mainDescSets_[i];
        writes[0].dstBinding      = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo     = &bi;

        writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet          = mainDescSets_[i];
        writes[1].dstBinding      = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo      = &texInfo;

        writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet          = mainDescSets_[i];
        writes[2].dstBinding      = 2;
        writes[2].descriptorCount = 1;
        writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[2].pImageInfo      = &nrmInfo;

        vkUpdateDescriptorSets(ctx.getDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void DescriptorManager::createBoxSets(const VulkanContext& ctx, const PipelineManager& pipelineMgr)
{
    const auto count = static_cast<uint32_t>(ubos_.size());
    std::vector<VkDescriptorSetLayout> layouts(count, pipelineMgr.getBoxDescSetLayout());

    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = pool_;
    ai.descriptorSetCount = count;
    ai.pSetLayouts        = layouts.data();

    boxDescSets_.resize(count);
    if (vkAllocateDescriptorSets(ctx.getDevice(), &ai, boxDescSets_.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate box descriptor sets!");

    for (uint32_t i = 0; i < count; ++i) {
        VkDescriptorBufferInfo bi{};
        bi.buffer = ubos_[i]; bi.offset = 0; bi.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet w{};
        w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet          = boxDescSets_[i];
        w.dstBinding      = 0;
        w.descriptorCount = 1;
        w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo     = &bi;

        vkUpdateDescriptorSets(ctx.getDevice(), 1, &w, 0, nullptr);
    }
}
