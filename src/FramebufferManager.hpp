#pragma once
#include "VulkanContext.hpp"
#include "SwapChain.hpp"
#include "RenderPassManager.hpp"

class FramebufferManager {
public:
    void create(const VulkanContext& ctx, const SwapChain& swapChain, const RenderPassManager& rpMgr);
    void recreate(const VulkanContext& ctx, const SwapChain& swapChain, const RenderPassManager& rpMgr);
    void destroy(const VulkanContext& ctx);

    VkFramebuffer getFramebuffer(uint32_t index) const { return framebuffers_[index]; }
    VkFramebuffer getPickFramebuffer()           const { return pickFramebuffer_; }
    VkImage       getPickColorImage()            const { return pickColorImage_; }
    VkImageView   getPickColorImageView()        const { return pickColorImageView_; }
    VkImageView   getDepthImageView()            const { return depthImageView_; }

    VkImageView createImageView(const VulkanContext& ctx, VkImage image, VkFormat format,
                                VkImageAspectFlags aspect) const;
    void createImage(const VulkanContext& ctx, uint32_t w, uint32_t h, VkFormat fmt,
                     VkImageTiling tiling, VkImageUsageFlags usage,
                     VkMemoryPropertyFlags props, VkImage& img, VkDeviceMemory& mem) const;

private:
    VkImage        depthImage_{};
    VkDeviceMemory depthMemory_{};
    VkImageView    depthImageView_{};

    std::vector<VkFramebuffer> framebuffers_;

    VkImage        pickColorImage_{};
    VkDeviceMemory pickColorMemory_{};
    VkImageView    pickColorImageView_{};
    VkImage        pickDepthImage_{};
    VkDeviceMemory pickDepthMemory_{};
    VkImageView    pickDepthImageView_{};
    VkFramebuffer  pickFramebuffer_{};

    void createDepthResources(const VulkanContext& ctx, const SwapChain& swapChain,
                              const RenderPassManager& rpMgr);
    void createMainFramebuffers(const VulkanContext& ctx, const SwapChain& swapChain,
                                const RenderPassManager& rpMgr);
    void createPickResources(const VulkanContext& ctx, const SwapChain& swapChain,
                             const RenderPassManager& rpMgr);
    void destroyPickResources(const VulkanContext& ctx);
};
