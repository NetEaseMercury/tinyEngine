#include "VulkanContext.hpp"
#include "TinyEngineDebug.hpp"
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

void VulkanContext::init(GLFWwindow* window)
{
    createInstance();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
}

void VulkanContext::destroy()
{
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
}

float VulkanContext::getMaxAnisotropy() const
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    return props.limits.maxSamplerAnisotropy;
}

uint32_t VulkanContext::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type!");
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice dev) const
{
    QueueFamilyIndices indices;
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

    for (uint32_t i = 0; i < count; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport);
        if (presentSupport)
            indices.presentFamily = i;

        if (indices.isComplete()) break;
    }
    return indices;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice dev) const
{
    SwapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, surface_, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &formatCount, nullptr);
    if (formatCount) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &formatCount, details.formats.data());
    }

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &modeCount, nullptr);
    if (modeCount) {
        details.presentModes.resize(modeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &modeCount, details.presentModes.data());
    }
    return details;
}

void VulkanContext::createInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "tinyEngine";
    appInfo.pEngineName = "No Engine";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Enumerate the layers the Vulkan loader can see. This surfaces whether the
    // RenderDoc capture layer is discoverable at all (independent of whether it
    // will be auto-enabled), and lets us explicitly enable it below.
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    std::string layerReport = "Vulkan loader sees " + std::to_string(layerCount) + " instance layers:";
    bool renderdocLayerAvailable = false;
    for (const auto& lp : availableLayers) {
        layerReport += "\n  - " + std::string(lp.layerName);
        if (std::string(lp.layerName) == "VK_LAYER_RENDERDOC_Capture")
            renderdocLayerAvailable = true;
    }
    tinyengine::debug::appendRenderDocStatus(layerReport.c_str());

    // Build the enabled-layer list. RenderDoc's Vulkan capture layer refuses
    // explicit enable (returns VK_ERROR_INITIALIZATION_FAILED), so we rely on
    // the implicit-layer mechanism gated by the ENABLE_VULKAN_RENDERDOC_CAPTURE
    // env var. That env var MUST be set before the Vulkan loader is first
    // touched — the Editor does this in App.OnStartup before te_init runs.
    std::vector<const char*> layersToEnable;
    if (enableValidationLayers) {
        for (const char* n : validationLayers) layersToEnable.push_back(n);
    }
    if (renderdocLayerAvailable) {
        tinyengine::debug::appendRenderDocStatus(
            "VK_LAYER_RENDERDOC_Capture is discoverable; relying on implicit-layer auto-load "
            "(ENABLE_VULKAN_RENDERDOC_CAPTURE=1 must be set before process start).");
    } else {
        tinyengine::debug::appendRenderDocStatus("VK_LAYER_RENDERDOC_Capture NOT discoverable by the Vulkan loader.");
    }
    createInfo.enabledLayerCount   = static_cast<uint32_t>(layersToEnable.size());
    createInfo.ppEnabledLayerNames = layersToEnable.empty() ? nullptr : layersToEnable.data();

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (res != VK_SUCCESS) {
        tinyengine::debug::appendRenderDocStatus(
            ("vkCreateInstance failed (VkResult=" + std::to_string(res) + ")").c_str());
        throw std::runtime_error("Failed to create Vulkan instance!");
    }
}

void VulkanContext::createSurface(GLFWwindow* window)
{
    if (glfwCreateWindowSurface(instance_, window, nullptr, &surface_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface!");
}

void VulkanContext::pickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    for (const auto& d : devices) {
        if (isDeviceSuitable(d)) {
            physicalDevice_ = d;
            return;
        }
    }
    throw std::runtime_error("Failed to find Vulkan-supported GPU!");
}

void VulkanContext::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
    graphicsQueueFamilyIndex_ = indices.graphicsFamily.value();

    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily.value(), indices.presentFamily.value()
    };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount       = 1;
        qi.pQueuePriorities = &priority;
        queueInfos.push_back(qi);
    }

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos       = queueInfos.data();
    createInfo.pEnabledFeatures        = &features;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (enableValidationLayers) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device!");

    vkGetDeviceQueue(device_, indices.graphicsFamily.value(), 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, indices.presentFamily.value(),  0, &presentQueue_);
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice dev) const
{
    QueueFamilyIndices indices = findQueueFamilies(dev);
    bool extOk = checkDeviceExtensionSupport(dev);
    bool scOk  = false;
    if (extOk) {
        auto sc = querySwapChainSupport(dev);
        scOk = !sc.formats.empty() && !sc.presentModes.empty();
    }
    VkPhysicalDeviceFeatures feat{};
    vkGetPhysicalDeviceFeatures(dev, &feat);
    return indices.isComplete() && extOk && scOk && feat.samplerAnisotropy;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice dev)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, available.data());

    std::set<std::string> required(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto& ext : available)
        required.erase(ext.extensionName);
    return required.empty();
}

std::vector<const char*> VulkanContext::getRequiredExtensions()
{
    uint32_t glfwCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwCount);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    return extensions;
}
