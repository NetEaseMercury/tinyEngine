#pragma once
#include "VulkanContext.hpp"
#include "CommandManager.hpp"
#include "FramebufferManager.hpp"

class BufferManager {
public:
    void init(const VulkanContext& ctx, const CommandManager& cmdMgr);

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory) const;

    void copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) const;

    void uploadToDeviceLocal(const void* data, VkDeviceSize size,
                             VkBufferUsageFlags dstUsage,
                             VkBuffer& dstBuffer, VkDeviceMemory& dstMemory) const;

    void createVertexBuffer(const std::vector<struct Vertex>& vertices,
                            VkBuffer& outBuffer, VkDeviceMemory& outMemory) const;
    void createIndexBuffer(const std::vector<uint32_t>& indices,
                           VkBuffer& outBuffer, VkDeviceMemory& outMemory) const;

    void createInstanceBuffer(const void* data, VkDeviceSize size,
                              VkBuffer& outBuffer, VkDeviceMemory& outMemory) const;

    void destroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory) const;

private:
    const VulkanContext*   ctx_    = nullptr;
    const CommandManager*  cmdMgr_ = nullptr;
};
