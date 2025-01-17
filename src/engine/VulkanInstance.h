#pragma once
#include "../base/BaseInclude.hpp"
#include "camera.hpp"

#define WIDTH 1280 
#define HEIGHT 720

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char*> debugInstanceExtensions = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
const std::vector<const char*> validationLayers = {
		"VK_LAYER_KHRONOS_validation",
		"VK_LAYER_LUNARG_monitor"
};

const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

const int MAX_FRAMES_IN_FLIGHT = 2;

struct AllocatedImage {
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation vmaAllocation = 0;
	VkImageView view = VK_NULL_HANDLE;
	uint32_t mipLevels = 1;
};

struct AllocatedBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation vmaAllocation = 0;
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	[[nodiscard]] bool hasMandatoryFamilies() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};
struct Model {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	glm::mat4 modelMatrix; // 存储模型的变换矩阵
	VkBuffer vertexBuffer; // 每个模型的顶点缓冲区
	VkDeviceMemory vertexBufferMemory; // 顶点缓冲区的设备内存
	VkBuffer indexBuffer; // 每个模型的索引缓冲区
	VkDeviceMemory indexBufferMemory; // 索引缓冲区的设备内存
};


// move vulkan init to VulkanInstance
class VulkanInstance {

public:
	struct Properties {
		VkFormat swapChainImageFormat = VK_FORMAT_UNDEFINED;
		VkExtent2D swapChainExtent = { 0 };
		VkSampleCountFlagBits maxNbMsaaSamples = VK_SAMPLE_COUNT_1_BIT;
		float maxSamplerAnisotropy = 0.f;
	};

	VulkanInstance() = default;
	// 将以前vulkan 初始化相关API封装一下
public:
	void init(GLFWwindow* window);
	void createSwapChain();
	void cleanup();
	void cleanupSwapChain();
	void recreateSwapChain();
	void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, AllocatedImage& image);
	void copyDataToImage(VkCommandPool commandPool, uint32_t width, uint32_t height, uint32_t nbChannels,
		AllocatedImage& image, const void* data, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
	void destroyImage(AllocatedImage& image);
	void copyBufferToImage(VkCommandPool cmdPool, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
	void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, VkImageView& imageView, uint32_t baseMipLevel = 0) const;
	void generateMipmaps(VkCommandPool cmdPool, AllocatedImage& imageData, VkFormat imageFormat, int32_t texWidth, int32_t texHeight);
	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& buffer, uint32_t minAlignment = 0);
	void createGPUBufferFromCPUData(VkCommandPool cmdPool, VkDeviceSize size, VkBufferUsageFlags usage, const void* data, AllocatedBuffer& buffer);
	void copyDataToBuffer(uint32_t size, AllocatedBuffer& buffer, const void* data, uint32_t offset = 0);
	void destroyBuffer(AllocatedBuffer& buffer);
	void copyBufferToBuffer(VkCommandPool cmdPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
	size_t padUniformBufferSize(size_t originalSize);
	void* mapBuffer(AllocatedBuffer& buffer);
	void unmapBuffer(AllocatedBuffer& buffer);
	VkCommandBuffer beginSingleTimeCommands(VkCommandPool& commandPool);
	void endSingleTimeCommands(VkCommandBuffer commandBuffer, VkCommandPool& commandPool);
	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features) const;
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

	// get class property
	const Properties& getProperties() const;
	VkDevice& getLogicalDevice();
	const QueueFamilyIndices& getQueueFamilyIndices() const;
	const std::vector<VkImageView>& getSwapChainImageViews() const;
	const std::vector<VkImage>& getSwapChainImages() const;
	std::vector<VkImage>& getSwapChainImages();
	VkQueue& getGraphicsQueue();
	VkQueue& getPresentationQueue();
	VkSwapchainKHR& getSwapChain();
	VkPhysicalDevice& getPhysicalDevice();
	VkInstance& getInstance();
	size_t getSwapChainSize() const;
private:
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev);
	bool isDeviceSuitable(VkPhysicalDevice dev);
	bool checkDeviceExtensionSupport(VkPhysicalDevice dev);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkCommandBufferAllocateInfo createCommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t nbCommandBuffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	SwapChainSupportDetails querySwapChainSupportDetails(VkPhysicalDevice device);

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

	static std::vector<const char*> getRequiredExtensions();


	GLFWwindow*					window = nullptr;
	VkInstance					vulkan = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT	debugMessenger = VK_NULL_HANDLE;
	VkSurfaceKHR				surface = VK_NULL_HANDLE;

	// Device
	VkPhysicalDevice			physicalDevice = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties	physicalDeviceProperties = {};
	QueueFamilyIndices			queueFamilyIndices;
	SwapChainSupportDetails		swapChainSupportDetails;
	VkDevice					device = VK_NULL_HANDLE;
	VkQueue						graphicsQueue = VK_NULL_HANDLE;
	VkQueue						presentationQueue = VK_NULL_HANDLE;
								
	// Swap chain				
	VkSwapchainKHR				swapChain = VK_NULL_HANDLE;
	std::vector<VkImage>		swapChainImages;
	std::vector<VkImageView>	swapChainImageViews;

	// Allocator
	VmaAllocator				allocator;

	Properties					properties;


	// error check
	void VK_CHECK(VkResult err) {
		
		if (err) {
			throw std::runtime_error(std::to_string(err));
		}
	}
};