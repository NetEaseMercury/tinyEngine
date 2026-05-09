#pragma once
#include "VulkanContext.hpp"
#include "SwapChain.hpp"

class RenderPassManager {
public:
    void create(const VulkanContext& ctx, const SwapChain& swapChain);
    void destroy(const VulkanContext& ctx);

    VkRenderPass getMainRenderPass() const { return mainRenderPass_; }
    VkRenderPass getPickRenderPass() const { return pickRenderPass_; }

    VkFormat findDepthFormat(const VulkanContext& ctx) const;
    VkFormat findSupportedFormat(const VulkanContext& ctx,
        const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features) const;

private:
    VkRenderPass mainRenderPass_{};
    VkRenderPass pickRenderPass_{};

    void createMainRenderPass(const VulkanContext& ctx, VkFormat colorFormat);
    void createPickRenderPass(const VulkanContext& ctx);
};
