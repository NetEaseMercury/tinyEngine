#pragma once

#include "vectex.hpp"
#include "camera.hpp"
#include "iostream"
#include <optional>
#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#define WIDTH 2560
#define HEIGHT 1440

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> validationLayers = {
        "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::string defalultVertexShaderPath = "D:/Code/OpenGL/tinyEngine/res/shaders/vert.spv";
const std::string defalultFragShdaerPath = "D:/Code/OpenGL/tinyEngine/res/shaders/frag.spv";

const std::string defalultModelPath = "D:/Code/OpenGL/tinyEngine/res/models/cyber_room.obj";
const std::string defalultTexturePath = "D:/Code/OpenGL/tinyEngine/res/textures/cyber_room.png";

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    [[nodiscard]] bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanRender {
public:
    VulkanRender() = default;
    void Run();
    void Escape();

private:
    GLFWwindow* window = nullptr;

    VkInstance instance_{};
    VkSurfaceKHR surface{};
    ImGui_ImplVulkan_InitInfo guiInfo{};
    // �����豸����
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device{};


    // swapchain����
    // ͼ���������
    VkQueue graphicsQueue{};
    // �����������
    VkQueue presentQueue{};

    // �������������
    VkSwapchainKHR swapChain{};
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat{};
    VkExtent2D swapChainExtent{};
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    //��Ⱦ���̶��󻺴�
    VkRenderPass renderPass{};
    VkDescriptorSetLayout descriptorSetLayout{};
    VkPipelineLayout pipelineLayout{};
    VkPipeline graphicsPipeline{};

    VkCommandPool commandPool{};

    VkImage depthImage{};
    VkDeviceMemory depthImageMemory{};
    VkImageView depthImageView{};

    VkImage textureImage{};
    VkDeviceMemory textureImageMemory{};
    VkImageView textureImageView{};
    VkSampler textureSampler{};

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    VkBuffer vertexBuffer{};
    VkDeviceMemory vertexBufferMemory{};
    VkBuffer indexBuffer{};
    VkDeviceMemory indexBufferMemory{};

    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    VkDescriptorPool descriptorPool{};
    std::vector<VkDescriptorSet> descriptorSets{};

    std::vector<VkCommandBuffer> commandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;

    bool framebufferResized = false;

    std::unordered_map<std::string, std::vector<std::string>> shaderPath;

    float lastFrame = 0.0f;
private:
    void gameLoop();
    void DrawUI();
    void initGLFW();
    void initIMGUI();
    void initVulkan();
    void cleanUp();

    // vulkan method
    void createVulkanInstance();
    void createSurface();
    void setPhysicalDevice();
    void setLogicalDevice();
    // ������������������ƣ�����������
    void createVulkanSwapChain();

    void createVulkanImageViews();
    void createVulkanRenderPass();
    void createVulkanDescriptorSetLayout();

    // ֧�ֶ�����.spv�ļ�ֱ�Ӷ�ȡ
    void createVulkanGraphicsPipeline(std::string vertSpv, std::string fragSpv);
    void createVulkanFramebuffers();
    void createVulkanCommandPool();

    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer&
        buffer, VkDeviceMemory& bufferMemory);
    void createTextureImage(std::string texturePath);
    void createTextureImageView();
    void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& img,
        VkDeviceMemory& imageMemory);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void createVertexBuffer();
    void createIndexBuffer();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    void createCommandPool();
    void createCommandBuffers();
    void createFramebuffers();
    void createDepthResources();
    void createSyncObjects();
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features);
    void createVulkanSyncObjects();

    void recreateSwapChain();

    void cleanupVulkanSwapChain();

    void selectPhysicalDevice();

    void updateUniformBuffer(uint32_t currentImage);

    void drawFrame();
    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev);

    bool isDeviceSuitable(VkPhysicalDevice dev);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkShaderModule createShaderModule(const std::vector<char>& code);
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    void loadModel(std::string modelPath);

    // static functions
    static void checkExtensions();
    static std::vector<char> readFile(const std::string& fileName);
    static bool checkDeviceExtensionSupport(VkPhysicalDevice dev);
    static std::vector<const char*> getRequiredExtensions();
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    void createTextureSampler();
    VkFormat findDepthFormat();
    bool hasStencilComponent(VkFormat format);
};