#include "DescriptorManager.hpp"
#include "VulkanTypes.hpp"
#include <cstring>
#include <stdexcept>

void DescriptorManager::create(const VulkanContext& ctx, const SwapChain& swapChain,
                                const PipelineManager& pipelineMgr, const BufferManager& bufMgr)
{
    const uint32_t count = swapChain.getImageCount();
    createUniformBuffers(ctx, count, bufMgr);
    createPool(ctx, count);
    createBoxSets(ctx, pipelineMgr);
}

void DescriptorManager::recreate(const VulkanContext& ctx, const SwapChain& swapChain,
                                  const PipelineManager& pipelineMgr, const BufferManager& bufMgr)
{
    destroy(ctx);
    create(ctx, swapChain, pipelineMgr, bufMgr);
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
    boxDescSets_.clear();
}

void DescriptorManager::updateUniformBuffer(uint32_t frameIndex,
                                             const glm::mat4& view, const glm::mat4& proj)
{
    UniformBufferObject ubo{};
    ubo.view            = view;
    ubo.proj            = proj;
    ubo.materialTint    = glm::vec4(1.f);
    ubo.boxMaterialTint = glm::vec4(1.f);
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
    VkDescriptorPoolSize size{};
    size.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    size.descriptorCount = imageCount;

    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = 1;
    ci.pPoolSizes    = &size;
    ci.maxSets       = imageCount;
    if (vkCreateDescriptorPool(ctx.getDevice(), &ci, nullptr, &pool_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool!");
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
