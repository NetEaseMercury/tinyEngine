#include "../base/BaseInclude.hpp"
class UIManager {
public:
	UIManager() = default;
	virtual ~UIManager() = default;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
public:
	ImGui_ImplVulkanH_Window IMGUIWindowData{};
public:
	void initIMGUI();
	void setVulkanInstance(const VkInstance& instance, VkAllocationCallbacks* allocator);
	void cleanUp();
	void startNewFrame();
	void setPhysicalDevice(const VkDevice& device, const VkPhysicalDevice& physicalDevice);
	bool getVulkanRefreshState();
	void setVulkanRefreshStatu(bool status);
	bool getFrustumCullingState();
	bool getOcclusionCullingState();
	std::string getScenePath();
	void getZDistance(float& zNear, float& zFar);
	void getCullingState(bool& enableFrus, bool& enableOC);
private:
	void setIMGUIVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height);
	void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data);
	void FramePresent(ImGui_ImplVulkanH_Window* wd);
	void setVulkan();

	VkAllocationCallbacks*   Allocator = nullptr;
	VkInstance               Instance = VK_NULL_HANDLE;
	VkPhysicalDevice         PhysicalDevice = VK_NULL_HANDLE;
	VkDevice                 Device = VK_NULL_HANDLE;
	uint32_t                 QueueFamily = 0;
	VkQueue                  Queue = VK_NULL_HANDLE;
	VkDebugReportCallbackEXT DebugReport = VK_NULL_HANDLE;
	VkPipelineCache          PipelineCache = VK_NULL_HANDLE;
	VkDescriptorPool         DescriptorPool = VK_NULL_HANDLE;
	ImGuiIO g_io;
	ImGui_ImplVulkanH_Window MainWindowData;
	int                      MinImageCount = 2;
	bool					 SwapChainRebuild = false;
	GLFWwindow* window = nullptr;
	VkSurfaceKHR surface;
	bool refreshVulkanRender = false;
	float _zNear = 0.1f;
	float _zFar = 300.0f;
	int currentIndex = 0;
	bool frustumCulling = true;
	bool occlusionCulling = true;

	std::string scenePath = "res/Models/Sponza/super_sponza.scene";
};