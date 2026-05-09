#include "TextureManager.hpp"
#include <stb_image.h>
#include <stdexcept>
#include <cstring>

void TextureManager::loadTexture(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                 const FramebufferManager& fbMgr, const std::string& path)
{
    loadImage(ctx, cmdMgr, fbMgr, path,
              textureImage_, textureMemory_, textureImageView_, textureSampler_,
              "Failed to load texture image!");
}

void TextureManager::loadNormalMap(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                   const FramebufferManager& fbMgr, const std::string& path)
{
    loadImage(ctx, cmdMgr, fbMgr, path,
              normalImage_, normalMemory_, normalImageView_, normalSampler_,
              "Failed to load normal map image!");
}

void TextureManager::destroy(const VulkanContext& ctx)
{
    auto dev = ctx.getDevice();
    auto destroyOne = [&](VkSampler& s, VkImageView& v, VkImage& i, VkDeviceMemory& m) {
        if (s != VK_NULL_HANDLE) { vkDestroySampler(dev, s, nullptr);    s = VK_NULL_HANDLE; }
        if (v != VK_NULL_HANDLE) { vkDestroyImageView(dev, v, nullptr);  v = VK_NULL_HANDLE; }
        if (i != VK_NULL_HANDLE) { vkDestroyImage(dev, i, nullptr);      i = VK_NULL_HANDLE; }
        if (m != VK_NULL_HANDLE) { vkFreeMemory(dev, m, nullptr);        m = VK_NULL_HANDLE; }
    };
    destroyOne(normalSampler_,  normalImageView_,  normalImage_,  normalMemory_);
    destroyOne(textureSampler_, textureImageView_, textureImage_, textureMemory_);
}

void TextureManager::loadImage(const VulkanContext& ctx, const CommandManager& cmdMgr,
                               const FramebufferManager& fbMgr, const std::string& path,
                               VkImage& outImage, VkDeviceMemory& outMemory,
                               VkImageView& outView, VkSampler& outSampler,
                               const char* errorMsg)
{
    int w = 0, h = 0, ch = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!pixels) throw std::runtime_error(errorMsg);

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(w * h * 4);

    VkBuffer       staging{};
    VkDeviceMemory stagingMem{};
    {
        VkBufferCreateInfo bi{};
        bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bi.size        = imageSize;
        bi.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(ctx.getDevice(), &bi, nullptr, &staging);

        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(ctx.getDevice(), staging, &req);
        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = req.size;
        ai.memoryTypeIndex = ctx.findMemoryType(req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &stagingMem);
        vkBindBufferMemory(ctx.getDevice(), staging, stagingMem, 0);
    }

    void* data = nullptr;
    vkMapMemory(ctx.getDevice(), stagingMem, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(ctx.getDevice(), stagingMem);
    stbi_image_free(pixels);

    fbMgr.createImage(ctx, static_cast<uint32_t>(w), static_cast<uint32_t>(h),
        VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, outImage, outMemory);

    transitionImageLayout(ctx, cmdMgr, outImage,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(ctx, cmdMgr, staging, outImage,
        static_cast<uint32_t>(w), static_cast<uint32_t>(h));
    transitionImageLayout(ctx, cmdMgr, outImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(ctx.getDevice(), staging, nullptr);
    vkFreeMemory(ctx.getDevice(), stagingMem, nullptr);

    outView = fbMgr.createImageView(ctx, outImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    createSampler(ctx, outSampler);
}

void TextureManager::createSampler(const VulkanContext& ctx, VkSampler& sampler)
{
    VkSamplerCreateInfo si{};
    si.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter        = VK_FILTER_LINEAR;
    si.minFilter        = VK_FILTER_LINEAR;
    si.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy    = ctx.getMaxAnisotropy();
    si.borderColor      = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    si.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(ctx.getDevice(), &si, nullptr, &sampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler!");
}

void TextureManager::transitionImageLayout(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                           VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkCommandBuffer cb = cmdMgr.beginSingleTimeCommands(ctx);

    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkPipelineStageFlags srcStage{}, dstStage{};
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::invalid_argument("Unsupported layout transition!");
    }

    vkCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    cmdMgr.endSingleTimeCommands(ctx, cb);
}

void TextureManager::copyBufferToImage(const VulkanContext& ctx, const CommandManager& cmdMgr,
                                       VkBuffer buffer, VkImage image, uint32_t w, uint32_t h)
{
    VkCommandBuffer cb = cmdMgr.beginSingleTimeCommands(ctx);

    VkBufferImageCopy region{};
    region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.imageExtent      = { w, h, 1 };
    vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    cmdMgr.endSingleTimeCommands(ctx, cb);
}
