#include "BufferManager.hpp"
#include "vectex.hpp"
#include <cstring>
#include <stdexcept>

void BufferManager::init(const VulkanContext& ctx, const CommandManager& cmdMgr)
{
    ctx_    = &ctx;
    cmdMgr_ = &cmdMgr;
}

void BufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                 VkMemoryPropertyFlags properties,
                                 VkBuffer& buffer, VkDeviceMemory& memory) const
{
    VkBufferCreateInfo bi{};
    bi.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bi.size        = size;
    bi.usage       = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(ctx_->getDevice(), &bi, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer!");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(ctx_->getDevice(), buffer, &req);

    VkMemoryAllocateInfo ai{};
    ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize  = req.size;
    ai.memoryTypeIndex = ctx_->findMemoryType(req.memoryTypeBits, properties);

    if (vkAllocateMemory(ctx_->getDevice(), &ai, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory!");

    vkBindBufferMemory(ctx_->getDevice(), buffer, memory, 0);
}

void BufferManager::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const
{
    VkCommandBuffer cb = cmdMgr_->beginSingleTimeCommands(*ctx_);
    VkBufferCopy region{ 0, 0, size };
    vkCmdCopyBuffer(cb, src, dst, 1, &region);
    cmdMgr_->endSingleTimeCommands(*ctx_, cb);
}

void BufferManager::uploadToDeviceLocal(const void* data, VkDeviceSize size,
                                        VkBufferUsageFlags dstUsage,
                                        VkBuffer& dstBuffer, VkDeviceMemory& dstMemory) const
{
    VkBuffer       staging{};
    VkDeviceMemory stagingMem{};
    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging, stagingMem);

    void* mapped = nullptr;
    vkMapMemory(ctx_->getDevice(), stagingMem, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(ctx_->getDevice(), stagingMem);

    createBuffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | dstUsage,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, dstBuffer, dstMemory);
    copyBuffer(staging, dstBuffer, size);

    vkDestroyBuffer(ctx_->getDevice(), staging, nullptr);
    vkFreeMemory(ctx_->getDevice(), stagingMem, nullptr);
}

void BufferManager::createVertexBuffer(const std::vector<Vertex>& vertices,
                                       VkBuffer& outBuffer, VkDeviceMemory& outMemory) const
{
    VkDeviceSize size = sizeof(vertices[0]) * vertices.size();
    uploadToDeviceLocal(vertices.data(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, outBuffer, outMemory);
}

void BufferManager::createIndexBuffer(const std::vector<uint32_t>& indices,
                                      VkBuffer& outBuffer, VkDeviceMemory& outMemory) const
{
    VkDeviceSize size = sizeof(indices[0]) * indices.size();
    uploadToDeviceLocal(indices.data(), size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, outBuffer, outMemory);
}

void BufferManager::createInstanceBuffer(const void* data, VkDeviceSize size,
                                         VkBuffer& outBuffer, VkDeviceMemory& outMemory) const
{
    createBuffer(size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 outBuffer, outMemory);

    void* mapped = nullptr;
    vkMapMemory(ctx_->getDevice(), outMemory, 0, size, 0, &mapped);
    memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(ctx_->getDevice(), outMemory);
}

void BufferManager::destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) const
{
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(ctx_->getDevice(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(ctx_->getDevice(), memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}
