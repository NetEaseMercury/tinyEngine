#pragma once
#include "VulkanTypes.hpp"
#include <string>

class VulkanContext {
public:
    void init(GLFWwindow* window);
    void destroy();

    VkInstance         getInstance()            const { return instance_; }
    VkDevice           getDevice()              const { return device_; }
    VkPhysicalDevice   getPhysicalDevice()      const { return physicalDevice_; }
    VkQueue            getGraphicsQueue()       const { return graphicsQueue_; }
    VkQueue            getPresentQueue()        const { return presentQueue_; }
    VkSurfaceKHR       getSurface()             const { return surface_; }
    uint32_t           getGraphicsQueueFamily() const { return graphicsQueueFamilyIndex_; }

    float getMaxAnisotropy() const;
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev) const;

private:
    VkInstance       instance_{};
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice         device_{};
    VkQueue          graphicsQueue_{};
    VkQueue          presentQueue_{};
    VkSurfaceKHR     surface_{};
    uint32_t         graphicsQueueFamilyIndex_{};

    void createInstance();
    void createSurface(GLFWwindow* window);
    void pickPhysicalDevice();
    void createLogicalDevice();

    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    static bool checkDeviceExtensionSupport(VkPhysicalDevice dev);
    static std::vector<const char*> getRequiredExtensions();
};
