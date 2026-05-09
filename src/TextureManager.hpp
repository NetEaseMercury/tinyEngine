#pragma once
#include "VulkanContext.hpp"
#include "CommandManager.hpp"
#include "FramebufferManager.hpp"
#include <string>

class TextureManager {
public:
    void loadTexture(const VulkanContext& ctx, const CommandManager& cmdMgr,
                     const FramebufferManager& fbMgr, const std::string& path);
    void loadNormalMap(const VulkanContext& ctx, const CommandManager& cmdMgr,
                       const FramebufferManager& fbMgr, const std::string& path);
    void destroy(const VulkanContext& ctx);

    VkImageView getTextureImageView() const { return textureImageView_; }
    VkImageView getNormalImageView()  const { return normalImageView_; }
    VkSampler   getTextureSampler()   const { return textureSampler_; }
    VkSampler   getNormalSampler()    const { return normalSampler_; }

private:
    VkImage        textureImage_{};
    VkDeviceMemory textureMemory_{};
    VkImageView    textureImageView_{};
    VkSampler      textureSampler_{};

    VkImage        normalImage_{};
    VkDeviceMemory normalMemory_{};
    VkImageView    normalImageView_{};
    VkSampler      normalSampler_{};

    void loadImage(const VulkanContext& ctx, const CommandManager& cmdMgr,
                   const FramebufferManager& fbMgr, const std::string& path,
                   VkImage& outImage, VkDeviceMemory& outMemory,
                   VkImageView& outView, VkSampler& outSampler,
                   const char* errorMsg);

    void createSampler(const VulkanContext& ctx, VkSampler& sampler);

    static void transitionImageLayout(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                      VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void copyBufferToImage(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                  VkBuffer buffer, VkImage image, uint32_t w, uint32_t h);
};
