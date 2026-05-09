#include "FramebufferManager.hpp"
#include <array>
#include <stdexcept>

void FramebufferManager::create(const VulkanContext& ctx, const SwapChain& swapChain,
                                const RenderPassManager& rpMgr)
{
    createDepthResources(ctx, swapChain, rpMgr);
    createMainFramebuffers(ctx, swapChain, rpMgr);
    createPickResources(ctx, swapChain, rpMgr);
}

void FramebufferManager::recreate(const VulkanContext& ctx, const SwapChain& swapChain,
                                  const RenderPassManager& rpMgr)
{
    destroy(ctx);
    create(ctx, swapChain, rpMgr);
}

void FramebufferManager::destroy(const VulkanContext& ctx)
{
    destroyPickResources(ctx);

    for (auto fb : framebuffers_)
        vkDestroyFramebuffer(ctx.getDevice(), fb, nullptr);
    framebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.getDevice(), depthImageView_, nullptr);
        depthImageView_ = VK_NULL_HANDLE;
    }
    if (depthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx.getDevice(), depthImage_, nullptr);
        depthImage_ = VK_NULL_HANDLE;
    }
    if (depthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.getDevice(), depthMemory_, nullptr);
        depthMemory_ = VK_NULL_HANDLE;
    }
}

VkImageView FramebufferManager::createImageView(const VulkanContext& ctx, VkImage image,
                                                VkFormat format, VkImageAspectFlags aspect) const
{
    VkImageViewCreateInfo vi{};
    vi.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vi.image    = image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format   = format;
    vi.subresourceRange.aspectMask     = aspect;
    vi.subresourceRange.baseMipLevel   = 0;
    vi.subresourceRange.levelCount     = 1;
    vi.subresourceRange.baseArrayLayer = 0;
    vi.subresourceRange.layerCount     = 1;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(ctx.getDevice(), &vi, nullptr, &view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view!");
    return view;
}

void FramebufferManager::createImage(const VulkanContext& ctx, uint32_t w, uint32_t h,
                                     VkFormat fmt, VkImageTiling tiling,
                                     VkImageUsageFlags usage, VkMemoryPropertyFlags props,
                                     VkImage& img, VkDeviceMemory& mem) const
{
    VkImageCreateInfo ii{};
    ii.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ii.imageType     = VK_IMAGE_TYPE_2D;
    ii.extent        = { w, h, 1 };
    ii.mipLevels     = 1;
    ii.arrayLayers   = 1;
    ii.format        = fmt;
    ii.tiling        = tiling;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ii.usage         = usage;
    ii.samples       = VK_SAMPLE_COUNT_1_BIT;
    ii.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(ctx.getDevice(), &ii, nullptr, &img) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image!");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(ctx.getDevice(), img, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = ctx.findMemoryType(req.memoryTypeBits, props);

    if (vkAllocateMemory(ctx.getDevice(), &ai, nullptr, &mem) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory!");

    vkBindImageMemory(ctx.getDevice(), img, mem, 0);
}

void FramebufferManager::createDepthResources(const VulkanContext& ctx, const SwapChain& swapChain,
                                              const RenderPassManager& rpMgr)
{
    VkFormat fmt = rpMgr.findDepthFormat(ctx);
    const auto ext = swapChain.getExtent();
    createImage(ctx, ext.width, ext.height, fmt, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage_, depthMemory_);
    depthImageView_ = createImageView(ctx, depthImage_, fmt, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void FramebufferManager::createMainFramebuffers(const VulkanContext& ctx, const SwapChain& swapChain,
                                               const RenderPassManager& rpMgr)
{
    const auto& views = swapChain.getImageViews();
    framebuffers_.resize(views.size());
    const auto ext = swapChain.getExtent();

    for (size_t i = 0; i < views.size(); ++i) {
        std::array<VkImageView, 2> atts{ views[i], depthImageView_ };

        VkFramebufferCreateInfo fbi{};
        fbi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbi.renderPass      = rpMgr.getMainRenderPass();
        fbi.attachmentCount = static_cast<uint32_t>(atts.size());
        fbi.pAttachments    = atts.data();
        fbi.width           = ext.width;
        fbi.height          = ext.height;
        fbi.layers          = 1;

        if (vkCreateFramebuffer(ctx.getDevice(), &fbi, nullptr, &framebuffers_[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer!");
    }
}

void FramebufferManager::createPickResources(const VulkanContext& ctx, const SwapChain& swapChain,
                                             const RenderPassManager& rpMgr)
{
    const auto ext = swapChain.getExtent();
    const uint32_t w = ext.width, h = ext.height;
    if (w == 0 || h == 0) return;

    createImage(ctx, w, h, VK_FORMAT_R32_UINT, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pickColorImage_, pickColorMemory_);
    pickColorImageView_ = createImageView(ctx, pickColorImage_, VK_FORMAT_R32_UINT, VK_IMAGE_ASPECT_COLOR_BIT);

    VkFormat depthFmt = rpMgr.findDepthFormat(ctx);
    createImage(ctx, w, h, depthFmt, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pickDepthImage_, pickDepthMemory_);
    pickDepthImageView_ = createImageView(ctx, pickDepthImage_, depthFmt, VK_IMAGE_ASPECT_DEPTH_BIT);

    std::array<VkImageView, 2> atts{ pickColorImageView_, pickDepthImageView_ };
    VkFramebufferCreateInfo fbi{};
    fbi.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbi.renderPass      = rpMgr.getPickRenderPass();
    fbi.attachmentCount = static_cast<uint32_t>(atts.size());
    fbi.pAttachments    = atts.data();
    fbi.width           = w;
    fbi.height          = h;
    fbi.layers          = 1;

    if (vkCreateFramebuffer(ctx.getDevice(), &fbi, nullptr, &pickFramebuffer_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pick framebuffer!");
}

void FramebufferManager::destroyPickResources(const VulkanContext& ctx)
{
    if (pickFramebuffer_ != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(ctx.getDevice(), pickFramebuffer_, nullptr);
        pickFramebuffer_ = VK_NULL_HANDLE;
    }
    if (pickColorImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.getDevice(), pickColorImageView_, nullptr);
        pickColorImageView_ = VK_NULL_HANDLE;
    }
    if (pickColorImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx.getDevice(), pickColorImage_, nullptr);
        pickColorImage_ = VK_NULL_HANDLE;
    }
    if (pickColorMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.getDevice(), pickColorMemory_, nullptr);
        pickColorMemory_ = VK_NULL_HANDLE;
    }
    if (pickDepthImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.getDevice(), pickDepthImageView_, nullptr);
        pickDepthImageView_ = VK_NULL_HANDLE;
    }
    if (pickDepthImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(ctx.getDevice(), pickDepthImage_, nullptr);
        pickDepthImage_ = VK_NULL_HANDLE;
    }
    if (pickDepthMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(ctx.getDevice(), pickDepthMemory_, nullptr);
        pickDepthMemory_ = VK_NULL_HANDLE;
    }
}
