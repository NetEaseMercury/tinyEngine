#include "IMGUIManager.hpp"
#include "Application.hpp"
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
/** @brief Error callback for the ImGui Vulkan backend: prints and aborts on non-zero (for learning; a release build could log instead) */
static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

/** @brief Return the exe directory for resolving relative paths such as res/shaders */
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

/** @brief Find the first existing model file in a directory by primary name plus fallbacks */
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

/** @brief Find a texture path in a directory by primary name plus the sample-texture list */
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

/** @brief See IMGUIManager.hpp: init ImGui_ImplVulkan with the current VulkanRender queue and RenderPass */
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
	init_info.RenderPass = vulkanRender->getMainRenderPass();
	init_info.Subpass = 0;
	init_info.MinImageCount = 2;
	init_info.ImageCount = vulkanRender->getSwapChainImageCount();
	init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = Allocator;
	init_info.CheckVkResultFn = check_vk_result;
	ImGui_ImplVulkan_Init(&init_info);
}

/** @brief See IMGUIManager.hpp: CreateContext, GLFW/Vulkan backends, dark theme */
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

/** @brief See IMGUIManager.hpp: rebind ImGui to the new RenderPass after swapchain recreation */
void UIManager::reloadImGuiVulkanAfterSwapchainRecreate(Application* app)
{
	vulkanRender = app;
	ImGui_ImplVulkan_Shutdown();
	initImGuiVulkanBackend();
}

/** @brief See IMGUIManager.hpp: store the Instance and allocator for later Init */
void UIManager::setVulkanInstance(const VkInstance& instance, VkAllocationCallbacks* allocator)
{
	Instance = instance;
	Allocator = allocator;
}

/** @brief See IMGUIManager.hpp: store PhysicalDevice and Device */
void UIManager::setPhysicalDevice(const VkDevice& device, const VkPhysicalDevice& physicalDevice)
{
	PhysicalDevice = physicalDevice;
	Device = device;
}

/** @brief See IMGUIManager.hpp: set whether a Vulkan refresh is requested (e.g. shader path changed) */
void UIManager::setRefreshVulkanStatus(bool status)
{
    refreshVulkanRender = status;
}

/** @brief See IMGUIManager.hpp: derive the box vertex shader path from the vert path */
void UIManager::syncBoxVertShaderPathFromVert()
{
    boxVertShaderPath = vertexShaderPath;
    static const char kVert[] = "vert.spv";
    const size_t pos = boxVertShaderPath.rfind(kVert);
    if (pos != std::string::npos)
        boxVertShaderPath.replace(pos, sizeof(kVert) - 1, "box_vert.spv");
}

/** @brief See IMGUIManager.hpp: derive the box fragment shader path from the frag path */
void UIManager::syncBoxFragShaderPathFromFrag()
{
    boxFragShaderPath = fragShaderPath;
    static const char kFrag[] = "frag.spv";
    const size_t pos = boxFragShaderPath.rfind(kFrag);
    if (pos != std::string::npos)
        boxFragShaderPath.replace(pos, sizeof(kFrag) - 1, "box.spv");
}

/** @brief See IMGUIManager.hpp: fill default resource directories and the current cyber_room sample paths */
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

/** @brief See IMGUIManager.hpp: whether recreateSwapChain / pipeline reload is needed */
bool UIManager::refreshVulkanShader()
{
    return refreshVulkanRender;
}

/** @brief See IMGUIManager.hpp: per-frame NewFrame, operation panel, ImGuizmo translate, ImGui::Render */
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


    ImGui::ColorEdit3("clear color", (float*)&clear_color); 

    // Static variable holding the selected item index
    static int selectedIndex = 0;

    // Define the drop-down items
    const char* items[] = { "cyberRoom", "fantasyGameInn", "vikingRoom"};
    const int itemCount = IM_ARRAYSIZE(items);

    // Create the drop-down combo box
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

    // ── Material Asset (.ast) Picker ─────────────────────────────────
    if (vulkanRender) {
        ImGui::Separator();
        ImGui::Text("Material Assets (.ast)");

        if (!astAssetScanned_) scanMaterialAssets();

        if (astAssetFiles_.empty()) {
            ImGui::TextDisabled("(no .ast files in res/materials)");
            if (ImGui::Button("Rescan##ast")) scanMaterialAssets();
        } else {
            std::vector<const char*> items;
            items.reserve(astAssetFiles_.size());
            for (const auto& s : astAssetFiles_) items.push_back(s.c_str());
            if (astAssetIndex_ < 0 || astAssetIndex_ >= static_cast<int>(items.size()))
                astAssetIndex_ = 0;
            ImGui::Combo("##astpick", &astAssetIndex_, items.data(), static_cast<int>(items.size()));

            if (ImGui::Button("Apply##ast")) {
                const std::string rel = std::string("materials/") + astAssetFiles_[astAssetIndex_];
                if (vulkanRender->loadAndApplyMaterialAsset(rel)) {
                    astStatusMsg_ = std::string("Loaded: ") + astAssetFiles_[astAssetIndex_];
                } else {
                    astStatusMsg_ = std::string("Failed: ") + astAssetFiles_[astAssetIndex_];
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Rescan##ast")) scanMaterialAssets();

            if (!astStatusMsg_.empty()) {
                ImGui::TextWrapped("%s", astStatusMsg_.c_str());
            }
        }
    }

    if (vulkanRender) {
        ImGui::Separator();
        ImGui::Text("Main model (scene mesh)");
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
            vulkanRender->removeBox(static_cast<Application::RenderEntityId>(deleteBoxId));
        }
    }

    // ── Material Panel ────────────────────────────────────────────────────────
    if (vulkanRender) {
        ImGui::Separator();
        ImGui::Text("Materials");

        MaterialManager& matMgr = vulkanRender->getMaterialManager();
        const auto& allIds = matMgr.getAllMaterialIds();

        // Sync editing selection to the picked object's material
        if (vulkanRender->selectedMaterialId != kInvalidMaterialId &&
            matMgr.isValid(vulkanRender->selectedMaterialId))
        {
            editingMaterialId_ = vulkanRender->selectedMaterialId;
        }
        if (!matMgr.isValid(editingMaterialId_) && !allIds.empty())
            editingMaterialId_ = allIds.front();

        // Material list
        if (ImGui::BeginListBox("##materials", ImVec2(-1, 80.f))) {
            for (MaterialId id : allIds) {
                const bool sel = (id == editingMaterialId_);
                char label[256];
                snprintf(label, sizeof(label), "[%u] %s (%s)",
                         id, matMgr.getMaterialName(id).c_str(),
                         matMgr.getMaterialType(id) == MaterialType::Mesh ? "Mesh" : "Box");
                if (ImGui::Selectable(label, sel)) {
                    editingMaterialId_ = id;
                    // Sync texture path buffers to this material
                    const std::string& ap = matMgr.getAlbedoPath(id);
                    const std::string& np = matMgr.getNormalPath(id);
                    snprintf(matAlbedoPath_, sizeof(matAlbedoPath_), "%s", ap.c_str());
                    snprintf(matNormalPath_, sizeof(matNormalPath_), "%s", np.c_str());
                }
            }
            ImGui::EndListBox();
        }

        // Create new material
        if (ImGui::Button("+ Mesh Material")) {
            MaterialId newId = vulkanRender->createMeshMaterial("New Mesh", "", "", MaterialParams{});
            editingMaterialId_ = newId;
            matAlbedoPath_[0] = '\0';
            matNormalPath_[0] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Box Material")) {
            MaterialId newId = vulkanRender->createBoxMaterial("New Box", MaterialParams{});
            editingMaterialId_ = newId;
        }

        // Edit selected material
        if (matMgr.isValid(editingMaterialId_)) {
            ImGui::Separator();
            ImGui::Text("Editing: [%u]", editingMaterialId_);
            MaterialParams& p = matMgr.getParamsMut(editingMaterialId_);

            char nameBuf[256];
            snprintf(nameBuf, sizeof(nameBuf), "%s", matMgr.getMaterialName(editingMaterialId_).c_str());
            if (ImGui::InputText("Name##mat", nameBuf, sizeof(nameBuf)))
                matMgr.setMaterialName(editingMaterialId_, nameBuf);

            ImGui::ColorEdit4("Base Color##mat",          &p.baseColor.x);
            ImGui::SliderFloat("Roughness##mat",          &p.roughness,          0.f, 1.f);
            ImGui::SliderFloat("Metallic##mat",           &p.metallic,           0.f, 1.f);
            ImGui::SliderFloat("Emissive Intensity##mat", &p.emissiveIntensity,  0.f, 10.f);
            ImGui::ColorEdit3("Emissive Color##mat",      &p.emissiveColor.x);

            if (matMgr.getMaterialType(editingMaterialId_) == MaterialType::Mesh) {
                ImGui::Separator();
                ImGui::Text("Textures (leave blank for default white/normal)");
                ImGui::InputText("Albedo##mat", matAlbedoPath_, sizeof(matAlbedoPath_));
                ImGui::SameLine();
                if (ImGui::Button("Load##albedo"))
                    vulkanRender->setMaterialAlbedo(editingMaterialId_, matAlbedoPath_);

                ImGui::InputText("Normal##mat", matNormalPath_, sizeof(matNormalPath_));
                ImGui::SameLine();
                if (ImGui::Button("Load##normal"))
                    vulkanRender->setMaterialNormal(editingMaterialId_, matNormalPath_);
            }

            ImGui::Separator();
            if (ImGui::Button("Assign to Selected")) {
                if (vulkanRender->mainModelSelected)
                    vulkanRender->setModelMaterial(editingMaterialId_);
                else if (vulkanRender->pickedBoxEntityId != 0)
                    vulkanRender->setBoxMaterial(vulkanRender->pickedBoxEntityId, editingMaterialId_);
            }

            if (matMgr.isDeletable(editingMaterialId_)) {
                ImGui::SameLine();
                if (ImGui::Button("Delete##mat")) {
                    vulkanRender->destroyMaterial(editingMaterialId_);
                    editingMaterialId_ = allIds.empty() ? kInvalidMaterialId : allIds.front();
                    matAlbedoPath_[0] = '\0';
                    matNormalPath_[0] = '\0';
                }
            }
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
			glm::mat4 model = vulkanRender->mainModelTransform.GetModelMatrix();
			ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), ImGuizmo::TRANSLATE, ImGuizmo::WORLD,
				glm::value_ptr(model), nullptr, nullptr);
			vulkanRender->mainModelTransform.position = glm::vec3(model[3]);
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

/** @brief See IMGUIManager.hpp: Shutdown ImGui Vulkan/GLFW and DestroyContext */
void UIManager::cleanUp()
{
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

/** @brief Scan .ast files under res/materials/ into astAssetFiles_ */
void UIManager::scanMaterialAssets()
{
    astAssetFiles_.clear();
    const std::filesystem::path matDir =
        std::filesystem::path(applicationResourceRoot()) / "res" / "materials";
    std::error_code ec;
    if (std::filesystem::is_directory(matDir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(matDir, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".ast") {
                astAssetFiles_.push_back(entry.path().filename().string());
            }
        }
        std::sort(astAssetFiles_.begin(), astAssetFiles_.end());
    }
    astAssetScanned_ = true;
    if (astAssetIndex_ >= static_cast<int>(astAssetFiles_.size()))
        astAssetIndex_ = 0;
}
