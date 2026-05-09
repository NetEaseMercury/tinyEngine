#pragma once
#include "VulkanContext.hpp"
#include "SwapChain.hpp"
#include "PipelineManager.hpp"
#include "BufferManager.hpp"
#include <glm/glm.hpp>
#include <vector>

// Owns only the box descriptor sets used by the pick pass.
// Per-material descriptor sets (mesh + box) are managed by MaterialManager.
class DescriptorManager {
public:
    void create(const VulkanContext& ctx, const SwapChain& swapChain,
                const PipelineManager& pipelineMgr, const BufferManager& bufMgr);
    void recreate(const VulkanContext& ctx, const SwapChain& swapChain,
                  const PipelineManager& pipelineMgr, const BufferManager& bufMgr);
    void destroy(const VulkanContext& ctx);

    void updateUniformBuffer(uint32_t frameIndex,
                             const glm::mat4& view, const glm::mat4& proj);

    VkDescriptorSet getBoxDescriptorSet(uint32_t index) const { return boxDescSets_[index]; }

private:
    VkDescriptorPool             pool_{};
    std::vector<VkDescriptorSet> boxDescSets_;
    std::vector<VkBuffer>        ubos_;
    std::vector<VkDeviceMemory>  uboMemory_;
    std::vector<void*>           uboMapped_;

    void createUniformBuffers(const VulkanContext& ctx, uint32_t count, const BufferManager& bufMgr);
    void createPool(const VulkanContext& ctx, uint32_t imageCount);
    void createBoxSets(const VulkanContext& ctx, const PipelineManager& pipelineMgr);
};
