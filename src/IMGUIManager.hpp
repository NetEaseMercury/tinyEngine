#pragma once

#include <imgui.h>

#include <backends/imgui_impl_vulkan.h>

#include <backends/imgui_impl_glfw.h>

#include <vector>

#include <GLFW/glfw3.h>

#include <string>

#include <cstdint>

class Application;

/**
 * @class UIManager
 * @brief Dear ImGui + ImGui_ImplVulkan/GLFW wrapper: per-frame UI, resource/scene
 *        coordination with VulkanRender, and the ImGuizmo translate handle.
 *
 * @details Learning pipeline: initIMGUI → (per frame) prepareFrame (NewFrame +
 * panels + Manipulate + ImGui::Render) → draw the ImGui draw data onto the
 * swapchain while recording the command buffer.
 */
class UIManager {

public:

	/** @brief Default constructor: members are mostly zero-initialized or null */
	UIManager() = default;

	/** @brief Virtual destructor: overridable; ImGui/Vulkan backends are explicitly released via cleanUp */
	virtual ~UIManager() = default;

	/** @brief ImGui clear color; editable via the panel ColorEdit3 and read back by the render path */
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	/**
	 * @brief Create the ImGui context and bind the GLFW and Vulkan backends; call after VulkanRender has created window/device/renderPass.
	 */
	void initIMGUI();

	/**
	 * @brief Store the VkInstance and optional allocator; Instance must be set before initImGuiVulkanBackend.
	 */
	void setVulkanInstance(const VkInstance& instance, VkAllocationCallbacks* allocator);

	/**
	 * @brief Destroy the ImGui Vulkan backend and release GLFW-related state; call before process exit or recreation.
	 */
	void cleanUp();

	/**
	 * @brief Called every frame before drawFrame recording: NewFrame, business windows, ImGuizmo::Manipulate, ImGui::Render.
	 */
	void prepareFrame();

	/**
	 * @brief After swapchain recreation: Shutdown the ImGui Vulkan backend and re-Init with the new RenderPass/ImageCount.
	 */
	void reloadImGuiVulkanAfterSwapchainRecreate(Application* app);

	/**
	 * @brief Set the logical and physical device handles for ImGui_ImplVulkan_Init.
	 */
	void setPhysicalDevice(const VkDevice& device, const VkPhysicalDevice& physicalDevice);

	/**
	 * @brief When true, the main loop should trigger recreateSwapChain (e.g. shader path changed).
	 */
	bool refreshVulkanShader();

	/**
	 * @brief Set whether Vulkan needs a refresh next frame (paired with the refreshVulkanShader read side).
	 */
	void setRefreshVulkanStatus(bool status);

	/**
	 * @brief Fill the default shader/model/texture path buffers from the res/ directory next to the exe.
	 */
	void setModelDefaultPath();

	/** @brief Attach the scene renderer so panels can read/write selection, matrices and resource paths */
	void setVulkanRender(Application* app) { vulkanRender = app; }

	/** @brief Return the current clear color (RGBA) */
	[[nodiscard]] ImVec4 getClearColor() const { return clear_color; }

	/** @brief Current main-model vertex shader SPIR-V path (syncable from the panel) */
	std::string vertexShaderPath;

	/** @brief Current main-model fragment shader SPIR-V path */
	std::string fragShaderPath;

	/** @brief Box fragment shader path (usually derived from the frag path as box.spv) */
	std::string boxFragShaderPath;

	/** @brief Box vertex shader SPIR-V path (derived from vert.spv as box_vert.spv) */
	std::string boxVertShaderPath;

	/** @brief Currently loaded OBJ model path */
	std::string modelPath;

	/** @brief Current main texture path */
	std::string texturePath;

private:


	/** @brief Generate boxFragShaderPath from fragShaderPath (frag.spv → box.spv) */
	void syncBoxFragShaderPathFromFrag();

	/** @brief Generate boxVertShaderPath from vertexShaderPath (vert.spv → box_vert.spv) */
	void syncBoxVertShaderPathFromVert();

	/** @brief Call ImGui_ImplVulkan_Init with the stored Instance/Device/Queue etc. */
	void initImGuiVulkanBackend();

	VkAllocationCallbacks* Allocator = nullptr;

	VkInstance Instance = VK_NULL_HANDLE;

	VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;

	VkDevice Device = VK_NULL_HANDLE;

	uint32_t QueueFamily = 0;

	VkQueue Queue = VK_NULL_HANDLE;

	VkPipelineCache PipelineCache = VK_NULL_HANDLE;

	/** @brief ImGui IO snapshot (used by some legacy code paths) */
	ImGuiIO g_io{};

	GLFWwindow* window = nullptr;

	/** @brief When true, refreshVulkanShader() returns true */
	bool refreshVulkanRender = false;

	/** @brief Vertex shader directory prefix + filename buffer */
	char VertexShaderPath[1024]{};

	char FragShdaerPath[1024]{};

	char ModelPath[1024]{};

	char TexturePath[1024]{};

	char currentVertexShaderPath[1024]{};

	char currentFragShdaerPath[1024]{};

	char currentModelPath[1024]{};

	char currentTexturePath[1024]{};

	int currentIndex = 0;

	float boxPosition[3] = { 0.0f, 0.0f, 0.0f };

	uint64_t lastAddedBoxId = 0;

	uint64_t deleteBoxId = 0;

	Application* vulkanRender = nullptr;

	// Material panel state
	uint32_t editingMaterialId_ = 0;
	char     matAlbedoPath_[1024]{};
	char     matNormalPath_[1024]{};

	// Material asset (.ast) picker state
	std::vector<std::string> astAssetFiles_;   // basenames under res/materials/, sorted
	int                      astAssetIndex_ = 0;
	bool                     astAssetScanned_ = false;
	std::string              astStatusMsg_;

	/** @brief Scan .ast files under res/materials/ into astAssetFiles_ */
	void scanMaterialAssets();

};
