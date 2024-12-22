#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>
#include <vector>
#include <GLFW/glfw3.h>
#include <string>
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
	bool refreshVulkanShader();
	float updateSpeed();
	void setRefreshVulkanStatus(bool status);
	void setModelDefaultPath();
public:

	std::string vertexShaderPath;
	std::string fragShaderPath;
	std::string modelPath;
	std::string texturePath;
public:
	float speed;
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

	char VertexShaderPath[1024];
	char FragShdaerPath[1024];
	char ModelPath[1024];
	char TexturePath[1024];

	char currentVertexShaderPath[1024];
	char currentFragShdaerPath[1024];
	char currentModelPath[1024];
	char currentTexturePath[1024];

	int currentIndex = -1;
};