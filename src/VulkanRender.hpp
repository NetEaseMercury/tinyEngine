#pragma once
/**
 * @file VulkanRender.hpp
 * @brief Vulkan renderer: swapchain, geometry, textures, UBO, pick pass, ImGui.
 *
 * @details Implementations: VulkanRender.cpp, VulkanRender_*.cpp (teaching: read by module).
 */
#include "VulkanRenderBase.hpp"
#include "vectex.hpp"
#include "camera.hpp"
#include "iostream"
#include <optional>
#include "IMGUIManager.hpp"
#include <unordered_map>
#include <cstdint>
/** @brief Default window width in pixels */
#define WIDTH 1280
/** @brief Default window height in pixels */
#define HEIGHT 720

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
/** @brief Enable Khronos validation layers in debug builds */
const bool enableValidationLayers = true;
#endif

/** @brief Layer names for VkInstance */
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};

/** @brief Required device extensions (includes swapchain) */
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

/** @brief Frames in flight (CPU-GPU sync) */
const int MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @brief Uniform buffer object: view, proj, tints (std140 aligned)
 */
struct UniformBufferObject {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::vec4 materialTint;
	alignas(16) glm::vec4 boxMaterialTint;
};

/**
 * @brief ?????????????????????
 */
struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;

	/** @brief ?????????????? true */
	[[nodiscard]] bool isComplete() const {
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

/**
 * @brief ????????????????????????????????????
 */
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> presentModes;
};

/**
 * @brief ???? Model ??????????????????????????? VBO/IBO???????????????????
 */
struct Model {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	/** @brief ?????????????????? */
	glm::mat4 modelMatrix;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
};

/**
 * @class VulkanRender
 * @brief ???????GLFW ?????Vulkan ???????????????????????????
 */
class VulkanRender : public VulkanRenderBase {
public:
	/** @brief ?????? ID??0 ??????????????? uint64_t */
	using RenderEntityId = uint64_t;

	VulkanRender() = default;

	/**
	 * @brief ???????? UIManager??initEngine?????? gameLoop??
	 */
	void Run() override;

	/**
	 * @brief ???? initGLFW??initVulkan??initIMGUI???????????????
	 */
	void initEngine() override;

	/**
	 * @brief ???????????????????????????? ImGui????????????????
	 */
	void Escape() override;

	/**
	 * @brief ?? OBJ ????????????????? modelVertices/modelIndices???????????????????????????
	 */
	void loadModel(std::string modelPath, glm::vec3 position) override;

	/**
	 * @brief ????????????????????????ImGui ???drawFrame??
	 */
	void gameLoop() override;

	/**
	 * @brief ??????????????????????????????????????? ID?????????????
	 */
	RenderEntityId addBox(const glm::vec3& position);

	/**
	 * @brief ?? ID ???????????????????????????????????
	 */
	bool removeBox(RenderEntityId id);

	/** @brief ???????????????????? ID ?????????/??????????????? */
	[[nodiscard]] glm::vec3 getBoxPosition(RenderEntityId id) const;

	/** @brief ?????????????????????????????? */
	void setBoxPosition(RenderEntityId id, const glm::vec3& worldPosition);

	/**
	 * @brief ???? framebuffer ??? (cursorX,cursorY) ?? R32_UINT ????????? mainModelSelected / pickedBoxEntityId??
	 */
	void tryPickMainModel(float cursorX, float cursorY);

	/** @brief ?? UBO ?????????? ImGuizmo ??? */
	glm::mat4 getSceneViewMatrix();

	/** @brief Vulkan ????????? Y ??? proj[1][1]*=-1?? */
	glm::mat4 getSceneProjMatrix();

	/**
	 * @brief ?????? ImGuizmo ?? OpenGL ??????????? Vulkan Y ????????????????????
	 */
	glm::mat4 getSceneProjMatrixForImGuizmo();

	static constexpr uint32_t kPickIdNone = 0;
	static constexpr uint32_t kPickIdMainModel = 1;
	static constexpr uint32_t kPickIdBoxBase = 2;

	/** @brief ???????????????? ImGuizmo ???????? */
	glm::vec3 mainModelPosition{ 0.0f, 0.0f, 0.0f };
	glm::vec3 materialTintRgb{ 1.0f, 1.0f, 1.0f };
	glm::vec3 boxMaterialTintRgb{ 1.0f, 1.0f, 1.0f };
	bool mainModelSelected = false;
	RenderEntityId pickedBoxEntityId = 0;

	[[nodiscard]] GLFWwindow* getMainWindow() const { return window; }
	[[nodiscard]] VkRenderPass getRenderPass() const { return renderPass; }
	[[nodiscard]] VkQueue getGraphicsQueue() const { return graphicsQueue; }
	[[nodiscard]] uint32_t getGraphicsQueueFamily() const { return graphicsQueueFamilyIndex_; }
	[[nodiscard]] uint32_t swapChainImageCount() const { return static_cast<uint32_t>(swapChainImages.size()); }

private:
	GLFWwindow* window = nullptr;
	VkInstance instance_{};
	VkSurfaceKHR surface{};
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device{};

	VkQueue graphicsQueue{};
	uint32_t graphicsQueueFamilyIndex_ = 0;
	VkQueue presentQueue{};

	VkSwapchainKHR swapChain{};
	std::vector<VkImage> swapChainImages;
	VkFormat swapChainImageFormat{};
	VkExtent2D swapChainExtent{};
	std::vector<VkImageView> swapChainImageViews;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	VkRenderPass renderPass{};
	VkDescriptorSetLayout descriptorSetLayout{};
	VkDescriptorSetLayout boxDescriptorSetLayout{};
	VkPipelineLayout pipelineLayout{};
	VkPipelineLayout boxPipelineLayout{};
	VkPipeline graphicsPipeline{};
	VkPipeline boxGraphicsPipeline{};

	VkCommandPool commandPool{};

	VkImage depthImage{};
	VkDeviceMemory depthImageMemory{};
	VkImageView depthImageView{};

	VkImage textureImage{};
	VkDeviceMemory textureImageMemory{};
	VkImageView textureImageView{};
	VkSampler textureSampler{};

	VkImage normalImage{};
	VkDeviceMemory normalImageMemory{};
	VkImageView normalImageView{};
	VkSampler normalSampler{};

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;

	std::vector<Vertex> modelVertices;
	std::vector<uint32_t> modelIndices;
	std::vector<Vertex> cubeTemplateVertices;
	std::vector<uint32_t> cubeTemplateIndices;
	std::unordered_map<RenderEntityId, glm::vec3> boxes;
	RenderEntityId nextRenderEntityId = 1;

	uint32_t modelIndexCount = 0;
	std::vector<RenderEntityId> boxRangeEntityIds;

	// Cube GPU buffers (static template, uploaded once)
	VkBuffer cubeVertexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory cubeVertexBufferMemory = VK_NULL_HANDLE;
	VkBuffer cubeIndexBuffer = VK_NULL_HANDLE;
	VkDeviceMemory cubeIndexBufferMemory = VK_NULL_HANDLE;
	uint32_t cubeIndexCount = 0;

	// Instance buffer (rebuilt when boxes change)
	VkBuffer instanceBuffer = VK_NULL_HANDLE;
	VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
	uint32_t instanceCount = 0;

	VkRenderPass pickRenderPass{};
	VkFramebuffer pickFramebuffer{};
	VkImage pickColorImage{};
	VkDeviceMemory pickColorImageMemory{};
	VkImageView pickColorImageView{};
	VkImage pickDepthImage{};
	VkDeviceMemory pickDepthImageMemory{};
	VkImageView pickDepthImageView{};
	VkPipelineLayout pickPipelineLayout{};
	VkPipeline pickPipeline{};
	VkCommandBuffer pickCommandBuffer{};
	VkBuffer pickReadbackBuffer{};
	VkDeviceMemory pickReadbackBufferMemory{};
	void* pickReadbackMapped = nullptr;

	VkBuffer vertexBuffer{};
	VkDeviceMemory vertexBufferMemory{};
	VkBuffer indexBuffer{};
	VkDeviceMemory indexBufferMemory{};

	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	VkDescriptorPool descriptorPool{};
	std::vector<VkDescriptorSet> descriptorSets{};
	std::vector<VkDescriptorSet> boxDescriptorSets;

	std::vector<VkCommandBuffer> commandBuffers;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	std::vector<VkFence> imagesInFlight;

	std::vector<Model> models;

	size_t currentFrame = 0;
	UIManager* imGUI = nullptr;
	bool framebufferResized = false;
	float lastFrame = 0.0f;

	/** @brief ???? GLFW ????????????/????? */
	void initGLFW();
	/** @brief ???? Instance????????????????????????????????? */
	void initVulkan();
	void initIMGUI();
	/** @brief ??? Vulkan ??????? */
	void cleanUp();

	void createVulkanInstance();
	void createSurface();
	void setPhysicalDevice();
	void setLogicalDevice();
	void createVulkanSwapChain();
	/** @brief ?????????????????????????????????????? swapchain ??????? */
	void recreateSwapChain();
	void cleanupVulkanSwapChain();

	/** @brief ?????acquire?????? UBO??submit??present */
	void drawFrame();
	/** @brief ????? swapchain ???????????? uniform buffer */
	void updateUniformBuffer(uint32_t currentImage);

	void createVulkanImageViews();
	void createVulkanRenderPass();
	void createVulkanDescriptorSetLayout();
	void createBoxDescriptorSetLayout();
	void createVulkanGraphicsPipeline(std::string vertSpv, std::string fragSpv);
	void createBoxGraphicsPipeline(std::string vertSpv, std::string boxFragSpv);
	void createVulkanFramebuffers();
	void createDescriptorSets();
	void createBoxDescriptorSets();

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory);
	void createVertexBuffer();
	void createIndexBuffer();
	void createUniformBuffers();
	void createCommandBuffers();
	void recordCommandBuffer(uint32_t imageIndex);
	void createFramebuffers();

	/** @brief ????? OBJ ???? AABB?????? F ??????? */
	glm::vec3 modelLocalBoundsMin{ 0.0f };
	glm::vec3 modelLocalBoundsMax{ 0.0f };

	void createTextureImage(std::string texturePath);
	void createNormalImage(std::string normalPath);

	void createTextureImageView();
	void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling,
		VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& img,
		VkDeviceMemory& imageMemory);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	void createVulkanCommandPool();
	void createDescriptorPool();
	void createCommandPool();

	void createDepthResources();
	void createSyncObjects();

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features);
	void createVulkanSyncObjects();

	std::unordered_map<std::string, std::vector<std::string>> modelsPath;
	VkCommandBuffer beginSingleTimeCommands();
	void endSingleTimeCommands(VkCommandBuffer commandBuffer);

	/** @brief Upload cubeTemplateVertices/Indices to DEVICE_LOCAL VBO/IBO */
	void createCubeGpuBuffers();
	/** @brief Safely destroy cube VBO/IBO */
	void destroyCubeGpuBuffers();
	/** @brief Rebuild HOST_VISIBLE instance buffer from sorted boxes map */
	void rebuildInstanceBuffer();
	/** @brief Safely destroy instance buffer */
	void destroyInstanceBuffer();

	/** @brief Fill cubeTemplateVertices/Indices with unit cube (half-extent 0.5) for instancing */
	void createCubeTemplate();
	/** @brief CPU ????????????????????????????? vertices/indices ?? boxIndexRanges */
	void rebuildCombinedGeometryCPU();
	void destroyGeometryBuffers();
	void recreateGeometryBuffersAndCommandBuffers();

	/** @brief ???????? color/depth??framebuffer???????? host-visible ??????? */
	void createPickPassResources();
	void destroyPickPassResources();
	/** @brief ?????????????????? UINT ???? ID ???? outObjectId */
	void runPickPassReadId(uint32_t pixelX, uint32_t pixelY, uint32_t& outObjectId);

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev);
	bool isDeviceSuitable(VkPhysicalDevice dev);
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice dev);
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void processInput(GLFWwindow* window);
	/** @brief ?? F ???????????????????????????? */
	void tryBeginCameraFocusOnPick();

	static std::vector<char> readFile(const std::string& fileName);
	static bool checkDeviceExtensionSupport(VkPhysicalDevice dev);
	static std::vector<const char*> getRequiredExtensions();
	static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
	static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
	void createTextureSampler();
	VkFormat findDepthFormat();
	bool hasStencilComponent(VkFormat format);
	bool useDefaultTexturePath = false;
};
