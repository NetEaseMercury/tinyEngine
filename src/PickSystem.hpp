#pragma once
#include "VulkanContext.hpp"
#include "CommandManager.hpp"
#include "RenderPassManager.hpp"
#include "FramebufferManager.hpp"
#include "PipelineManager.hpp"
#include "SceneManager.hpp"
#include <glm/glm.hpp>
#include <cstdint>

class PickSystem {
public:
    void create(const VulkanContext& ctx, const CommandManager& cmdMgr);
    void destroy(const VulkanContext& ctx, const CommandManager& cmdMgr);

    // Returns the raw pick ID, or SceneManager::kPickIdNone if nothing was hit.
    uint32_t runPick(const VulkanContext& ctx,
                     const RenderPassManager& rpMgr,
                     const FramebufferManager& fbMgr,
                     const PipelineManager& pipelineMgr,
                     VkDescriptorSet boxDescSet0,
                     const SceneManager& scene,
                     const glm::vec3& mainModelWorldPos,
                     VkExtent2D extent,
                     uint32_t pixelX, uint32_t pixelY);

private:
    VkCommandBuffer  pickCmdBuf_{};
    VkBuffer         readbackBuf_{};
    VkDeviceMemory   readbackMem_{};
    void*            readbackMapped_ = nullptr;
};
