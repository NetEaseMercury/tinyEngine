#include "IMGUIManager.hpp"
#include "VulkanRender.hpp"
#include "ImGuizmo.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstdio>
#include <filesystem>
/** @brief ImGui Vulkan 后端的错误回调：非 0 打印并中止（教学：发布版可改为日志） */
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

/** @brief 返回 exe 所在目录，用于拼接 res/shaders 等相对路径 */
static std::string applicationResourceRoot()
{
#ifdef _WIN32
	char path[MAX_PATH]{};
	if (GetModuleFileNameA(nullptr, path, MAX_PATH) != 0) {
		std::filesystem::path p(path);
		return p.parent_path().string();
	}
#endif
	return std::filesystem::current_path().string();
}

/** @brief 在目录中按主文件名与若干 fallback 查找第一个存在的模型文件 */
static std::string resolveModelInDir(const std::string& dirWithSlash, const char* primaryFile)
{
	const char* fallbacks[] = { primaryFile, "01.obj", "no_material.obj" };
	std::filesystem::path dir(dirWithSlash);
	std::error_code ec;
	for (const char* name : fallbacks) {
		const auto f = dir / name;
		if (std::filesystem::is_regular_file(f, ec))
			return f.string();
	}
	return (dir / primaryFile).string();
}

/** @brief 在目录中按主文件名与教学用示例贴图列表查找纹理路径 */
static std::string resolveTextureInDir(const std::string& dirWithSlash, const char* primaryFile)
{
	const char* fallbacks[] = {
		primaryFile,
		"cyber_room.png",
		"fantasy_game_inn.png",
		"viking_room.png",
		"texture.jpg",
	};
	std::filesystem::path dir(dirWithSlash);
	std::error_code ec;
	for (const char* name : fallbacks) {
		const auto f = dir / name;
		if (std::filesystem::is_regular_file(f, ec))
			return f.string();
	}
	return (dir / primaryFile).string();
}

/** @brief 见 IMGUIManager.hpp：用当前 VulkanRender 的队列与 RenderPass 初始化 ImGui_ImplVulkan */
void UIManager::initImGuiVulkanBackend()
{
	IM_ASSERT(vulkanRender != nullptr);
	QueueFamily = vulkanRender->getGraphicsQueueFamily();
	Queue = vulkanRender->getGraphicsQueue();

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = Instance;
	init_info.PhysicalDevice = PhysicalDevice;
	init_info.Device = Device;
	init_info.QueueFamily = QueueFamily;
	init_info.Queue = Queue;
	init_info.PipelineCache = PipelineCache;
	init_info.DescriptorPoolSize = 1024;
	init_info.RenderPass = vulkanRender->getRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = vulkanRender->swapChainImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = Allocator;
	init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info);
}

/** @brief 见 IMGUIManager.hpp：CreateContext、GLFW/Vulkan 后端、暗色主题 */
void UIManager::initIMGUI()
{
	IM_ASSERT(vulkanRender != nullptr);
	window = vulkanRender->getMainWindow();

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	g_io = io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForVulkan(window, true);
	initImGuiVulkanBackend();
}

/** @brief 见 IMGUIManager.hpp：swapchain 重建后重绑 ImGui 与新的 RenderPass */
void UIManager::reloadImGuiVulkanAfterSwapchainRecreate(VulkanRender* render)
{
	vulkanRender = render;
	ImGui_ImplVulkan_Shutdown();
	initImGuiVulkanBackend();
}

/** @brief 见 IMGUIManager.hpp：保存 Instance 与分配器供后续 Init */
void UIManager::setVulkanInstance(const VkInstance& instance, VkAllocationCallbacks* allocator)
{
	Instance = instance;
	Allocator = allocator;
}

/** @brief 见 IMGUIManager.hpp：保存 PhysicalDevice 与 Device */
void UIManager::setPhysicalDevice(const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
	PhysicalDevice = physicalDevice;
	Device = device;
}

/** @brief 见 IMGUIManager.hpp：设置是否请求刷新 Vulkan（如改 shader 路径） */
void UIManager::setRefreshVulkanStatus(bool status)
{
    refreshVulkanRender = status;
}

/** @brief 见 IMGUIManager.hpp：由 vert 路径派生 box 顶点 shader 路径 */
void UIManager::syncBoxVertShaderPathFromVert()
{
    boxVertShaderPath = vertexShaderPath;
    static const char kVert[] = "vert.spv";
    const size_t pos = boxVertShaderPath.rfind(kVert);
    if (pos != std::string::npos)
        boxVertShaderPath.replace(pos, sizeof(kVert) - 1, "box_vert.spv");
}

/** @brief 见 IMGUIManager.hpp：由 frag 路径派生 box 片段 shader 路径 */
void UIManager::syncBoxFragShaderPathFromFrag()
{
    boxFragShaderPath = fragShaderPath;
    static const char kFrag[] = "frag.spv";
    const size_t pos = boxFragShaderPath.rfind(kFrag);
    if (pos != std::string::npos)
        boxFragShaderPath.replace(pos, sizeof(kFrag) - 1, "box.spv");
}

/** @brief 见 IMGUIManager.hpp：填充默认资源目录与当前 cyber_room 示例路径 */
void UIManager::setModelDefaultPath()
{
	const std::string root = applicationResourceRoot();
	snprintf(VertexShaderPath, sizeof(VertexShaderPath), "%s/res/shaders/", root.c_str());
	snprintf(FragShdaerPath, sizeof(FragShdaerPath), "%s/res/shaders/", root.c_str());
	snprintf(ModelPath, sizeof(ModelPath), "%s/res/models/", root.c_str());
	snprintf(TexturePath, sizeof(TexturePath), "%s/res/textures/", root.c_str());
	snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
	snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);

	const std::string modelResolved = resolveModelInDir(ModelPath, "cyber_room.obj");
	snprintf(currentModelPath, sizeof(currentModelPath), "%s", modelResolved.c_str());
	const std::string texResolved = resolveTextureInDir(TexturePath, "cyber_room.png");
	snprintf(currentTexturePath, sizeof(currentTexturePath), "%s", texResolved.c_str());

	vertexShaderPath = currentVertexShaderPath;
	fragShaderPath = currentFragShdaerPath;
	syncBoxFragShaderPathFromFrag();
	syncBoxVertShaderPathFromVert();
	modelPath = modelResolved;
	texturePath = texResolved;
}

/** @brief 见 IMGUIManager.hpp：返回是否需要 recreateSwapChain / 重载管线 */
bool UIManager::refreshVulkanShader()
{
    return refreshVulkanRender;
}

/** @brief 见 IMGUIManager.hpp：返回 UI 上相机速度滑动条值 */
float UIManager::updateSpeed()
{
    return speed;
}

/** @brief 见 IMGUIManager.hpp：每帧 NewFrame、操作面板、ImGuizmo 平移、ImGui::Render */
void UIManager::prepareFrame()
{
	if (window == nullptr) {
		return;
	}

	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGuizmo::BeginFrame();

    ImGui::Begin("tinyEngineOperationWindow"); 
    std::string prompt= R"(
        You can move by pressing w, a, s, d. 
        You can press and hold the right mouse button to rotate the view.
    )";
    ImGui::Text(prompt.c_str());              
    ImGui::SliderFloat("Camera Move Speed", &speed, 0.0f, 1.0f);

    ImGui::ColorEdit3("clear color", (float*)&clear_color); 

    // 创建一个静态变量来存储选中项的索引
    static int selectedIndex = 0;

    // 定义下拉选项的内容
    const char* items[] = { "cyberRoom", "fantasyGameInn", "vikingRoom"};
    const int itemCount = IM_ARRAYSIZE(items);

    // 创建下拉选项框
    ImGui::Combo("drop-down box", &selectedIndex, items, itemCount);
    if (selectedIndex != currentIndex) {
        switch (selectedIndex)
        {
        case 0:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            {
                const std::string mr = resolveModelInDir(ModelPath, "cyber_room.obj");
                snprintf(currentModelPath, sizeof(currentModelPath), "%s", mr.c_str());
                const std::string tr = resolveTextureInDir(TexturePath, "cyber_room.png");
                snprintf(currentTexturePath, sizeof(currentTexturePath), "%s", tr.c_str());
            }
            setRefreshVulkanStatus(true);
            break;
        case 1:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            {
                const std::string mr = resolveModelInDir(ModelPath, "fantasy_game_inn.obj");
                snprintf(currentModelPath, sizeof(currentModelPath), "%s", mr.c_str());
                const std::string tr = resolveTextureInDir(TexturePath, "fantasy_game_inn.png");
                snprintf(currentTexturePath, sizeof(currentTexturePath), "%s", tr.c_str());
            }
            setRefreshVulkanStatus(true);
            break;
        case 2:
            snprintf(currentVertexShaderPath, sizeof(currentVertexShaderPath), "%svert.spv", VertexShaderPath);
            snprintf(currentFragShdaerPath, sizeof(currentFragShdaerPath), "%sfrag.spv", FragShdaerPath);
            {
                const std::string mr = resolveModelInDir(ModelPath, "viking_room.obj");
                snprintf(currentModelPath, sizeof(currentModelPath), "%s", mr.c_str());
                const std::string tr = resolveTextureInDir(TexturePath, "viking_room.png");
                snprintf(currentTexturePath, sizeof(currentTexturePath), "%s", tr.c_str());
            }
            setRefreshVulkanStatus(true);
            break;
        default:
            break;
        }
        vertexShaderPath = currentVertexShaderPath;
        fragShaderPath = currentFragShdaerPath;
        syncBoxFragShaderPathFromFrag();
        syncBoxVertShaderPathFromVert();
        modelPath = currentModelPath;
        texturePath = currentTexturePath;
        currentIndex = selectedIndex;
    }

    if (vulkanRender) {
        ImGui::Separator();
        ImGui::Text("Main model (scene mesh)");
        ImGui::ColorEdit3("Scene model RGB tint", glm::value_ptr(vulkanRender->materialTintRgb));
        ImGui::Text("Pick: left-click mesh or box in this window.");
        ImGui::Text("Main model selected: %s", vulkanRender->mainModelSelected ? "yes" : "no");
        ImGui::Text("Picked box entity: %llu",
            static_cast<unsigned long long>(vulkanRender->pickedBoxEntityId));
        if (vulkanRender->mainModelSelected) {
            ImGui::TextUnformatted("Move main: drag RGB arrows in 3D view.");
        }
        else if (vulkanRender->pickedBoxEntityId != 0) {
            ImGui::TextUnformatted("Move box: drag RGB arrows in 3D view.");
        }
    }

    ImGui::Separator();
    ImGui::Text("Box Entity");
    if (vulkanRender) {
        ImGui::ColorEdit3("Box RGB tint", glm::value_ptr(vulkanRender->boxMaterialTintRgb));
    }
    ImGui::InputFloat3("Box Position", boxPosition);
    if (ImGui::Button("Add Box")) {
        if (vulkanRender) {
            lastAddedBoxId = vulkanRender->addBox(glm::vec3(boxPosition[0], boxPosition[1], boxPosition[2]));
            deleteBoxId = lastAddedBoxId;
        }
    }
    ImGui::Text("Last Added ID: %llu", static_cast<unsigned long long>(lastAddedBoxId));
    ImGui::InputScalar("Delete Box ID", ImGuiDataType_U64, &deleteBoxId);
    if (ImGui::Button("Delete Box")) {
        if (vulkanRender) {
            vulkanRender->removeBox(static_cast<VulkanRender::RenderEntityId>(deleteBoxId));
        }
    }

    ImGui::End();

	if (vulkanRender != nullptr) {
		ImGuiIO& io = ImGui::GetIO();
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);
		glm::mat4 view = vulkanRender->getSceneViewMatrix();
		glm::mat4 proj = vulkanRender->getSceneProjMatrixForImGuizmo();
		if (vulkanRender->mainModelSelected) {
			glm::mat4 model = glm::translate(glm::mat4(1.0f), vulkanRender->mainModelPosition);
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
				glm::value_ptr(model), nullptr, nullptr);
			vulkanRender->mainModelPosition = glm::vec3(model[3]);
		}
		else if (vulkanRender->pickedBoxEntityId != 0) {
			glm::vec3 boxPos = vulkanRender->getBoxPosition(vulkanRender->pickedBoxEntityId);
			glm::mat4 model = glm::translate(glm::mat4(1.0f), boxPos);
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
				glm::value_ptr(model), nullptr, nullptr);
			vulkanRender->setBoxPosition(vulkanRender->pickedBoxEntityId, glm::vec3(model[3]));
		}
	}

    ImGui::Render();
}

/** @brief 见 IMGUIManager.hpp：Shutdown ImGui Vulkan/GLFW 并 DestroyContext */
void UIManager::cleanUp()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}
