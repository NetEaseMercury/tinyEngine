#pragma once
#include "VulkanContext.hpp"
#include "SwapChain.hpp"
#include "PipelineManager.hpp"
#include "TextureManager.hpp"
#include "BufferManager.hpp"
#include <glm/glm.hpp>
#include <vector>

class DescriptorManager {
public:
    void create(const VulkanContext& ctx, const SwapChain& swapChain,
                const PipelineManager& pipelineMgr, const TextureManager& textureMgr,
                const BufferManager& bufMgr);
    void recreate(const VulkanContext& ctx, const SwapChain& swapChain,
                  const PipelineManager& pipelineMgr, const TextureManager& textureMgr,
                  const BufferManager& bufMgr);
    void destroy(const VulkanContext& ctx);

    void updateUniformBuffer(uint32_t frameIndex,
                             const glm::mat4& view, const glm::mat4& proj,
                             const glm::vec3& materialTint, const glm::vec3& boxMaterialTint);

    VkDescriptorSet getMainDescriptorSet(uint32_t index) const { return mainDescSets_[index]; }
    VkDescriptorSet getBoxDescriptorSet(uint32_t index)  const { return boxDescSets_[index]; }

private:
    VkDescriptorPool             pool_{};
    std::vector<VkDescriptorSet> mainDescSets_;
    std::vector<VkDescriptorSet> boxDescSets_;
    std::vector<VkBuffer>        ubos_;
    std::vector<VkDeviceMemory>  uboMemory_;
    std::vector<void*>           uboMapped_;

    void createUniformBuffers(const VulkanContext& ctx, uint32_t count, const BufferManager& bufMgr);
    void createPool(const VulkanContext& ctx, uint32_t imageCount);
    void createMainSets(const VulkanContext& ctx, const PipelineManager& pipelineMgr,
                        const TextureManager& textureMgr);
    void createBoxSets(const VulkanContext& ctx, const PipelineManager& pipelineMgr);
};
