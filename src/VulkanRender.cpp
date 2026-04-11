/**
 * @file VulkanRender.cpp
 * @brief Application entry flow: GLFW callbacks, Run/initEngine/gameLoop, input, camera focus (F).
 * @see VulkanRender.hpp for full API documentation.
 */
#include "VulkanRender.hpp"
#include "VulkanRender_Globals.hpp"
#include "TinyEngineDebug.hpp"
#include "IMGUIManager.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <iostream>

Camera camera(glm::vec3(0.0f, -4.0f, 4.0f), glm::radians(45.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
bool firstMouse = true;
bool rightMouseStatus = false;
float lastX = WIDTH / 2.0f;
float lastY = HEIGHT / 2.0f;

/** @brief GLFW ????????????????????????????? ImGui ??????????? */
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	auto* app = reinterpret_cast<VulkanRender*>(glfwGetWindowUserPointer(window));
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
		rightMouseStatus = true;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
		rightMouseStatus = false;
	}
	if (app != nullptr && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
		if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) {
			return;
		}
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
			return;
		}
		double cx = 0.0;
		double cy = 0.0;
		glfwGetCursorPos(window, &cx, &cy);
		app->tryPickMainModel(static_cast<float>(cx), static_cast<float>(cy));
	}
}

/** @brief GLFW ???????????????????? camera.ProcessMouseMovement */
static void mouseCallback(GLFWwindow* window, double xPos, double yPos)
{
	if (ImGui::GetCurrentContext()) {
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse) {
			return;
		}
	}
	if (!rightMouseStatus) {
		return;
	}

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
		camera.speedZ = 1.0f;
	}
	if (firstMouse) {
		lastX = static_cast<float>(xPos);
		lastY = static_cast<float>(yPos);
		firstMouse = false;
	}
	const float deltaX = static_cast<float>(xPos) - lastX;
	const float deltaY = static_cast<float>(yPos) - lastY;
	lastX = static_cast<float>(xPos);
	lastY = static_cast<float>(yPos);
	camera.ProcessMouseMovement(deltaX, deltaY);
}

/** @brief ???? UIManager ?????? initEngine + gameLoop???? VulkanRender.hpp?? */
void VulkanRender::Run()
{
	imGUI = new UIManager();
	initEngine();
	gameLoop();
}

/** @brief GLFW?????????????????? Vulkan + ImGui ????? */
void VulkanRender::initEngine()
{
	tinyengine::debug::initRenderDoc();
	initGLFW();
	imGUI->setModelDefaultPath();
	initVulkan();
}

/** @brief ???? ImGui ?? Vulkan ??????????????????????? */
void VulkanRender::Escape()
{
	if (imGUI != nullptr) {
		imGUI->cleanUp();
	}
	cleanUp();
}

/** @brief ?????????????????????????ImGui ???drawFrame */
void VulkanRender::gameLoop()
{
	while (!glfwWindowShouldClose(window)) {
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			std::cout << "Exit Game" << std::endl;
			vkDeviceWaitIdle(device);
			Escape();
			break;
		}

		glfwPollEvents();
		float currentFrame = static_cast<float>(glfwGetTime());
		static float prevTickTime = -1.0f;
		const float deltaTime =
			(prevTickTime < 0.0f) ? 0.0f : glm::max(0.0f, currentFrame - prevTickTime);
		prevTickTime = currentFrame;
		if (currentFrame - lastFrame > 0.01f) {
			lastFrame = currentFrame;
		}
		else {
			lastFrame = currentFrame;
		}
		processInput(window);
		if (!camera.IsSmoothFocusActive()) {
			camera.UpdataCameraPosition();
		}
		camera.UpdateSmoothFocus(deltaTime > 0.0f ? deltaTime : 1.0f / 240.0f);
		imGUI->prepareFrame();
		drawFrame();
		camera.SPEED = imGUI->updateSpeed();
		if (imGUI->refreshVulkanShader()) {
			recreateSwapChain();
		}
		imGUI->setRefreshVulkanStatus(false);
	}
}

/** @brief ??????????? UserPointer ??????/????? */
void VulkanRender::initGLFW()
{
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(WIDTH, HEIGHT, "tinyEngine", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetCursorPosCallback(window, mouseCallback);
	glfwSetMouseButtonCallback(window, mouseButtonCallback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

/** @brief ?? Instance/Device/this ???? UIManager ?? initIMGUI */
void VulkanRender::initIMGUI()
{
	imGUI->setVulkanInstance(instance_, NULL);
	imGUI->setPhysicalDevice(device, physicalDevice);
	imGUI->setVulkanRender(this);
	imGUI->initIMGUI();
}

/** @brief ?????????????????????????????UBO????????????? */
void VulkanRender::initVulkan()
{
	createVulkanInstance();
	tinyengine::debug::initDebugUtils(instance_);
	createSurface();
	setPhysicalDevice();
	setLogicalDevice();
	createVulkanSwapChain();
	createVulkanImageViews();
	createVulkanRenderPass();
	createVulkanDescriptorSetLayout();
	createBoxDescriptorSetLayout();
	createVulkanGraphicsPipeline(imGUI->vertexShaderPath, imGUI->fragShaderPath);
	createBoxGraphicsPipeline(imGUI->vertexShaderPath, imGUI->boxFragShaderPath);
	createCommandPool();
	createDepthResources();
	createFramebuffers();
	createTextureImage(imGUI->texturePath);
	createTextureImageView();
	createTextureSampler();

	createNormalImage(imGUI->texturePath);
	normalImageView = createImageView(normalImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(physicalDevice, &properties);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (vkCreateSampler(device, &samplerInfo, nullptr, &normalSampler) != VK_SUCCESS) {
			throw std::runtime_error("failed to create normal sampler!");
		}
	}

	createCubeTemplate();
	loadModel(imGUI->modelPath, glm::vec3(0, 0, 0));

	createVertexBuffer();
	createIndexBuffer();

	createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
	createBoxDescriptorSets();

	initIMGUI();
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Render();

	createPickPassResources();
	createCommandBuffers();
	createSyncObjects();
}

/** @brief ???????? Vulkan ?????? GLFW?????? ImGui???? Escape ??? cleanUp?? */
void VulkanRender::cleanUp()
{
	destroyPickPassResources();

	vkDestroySampler(device, textureSampler, nullptr);
	vkDestroyImageView(device, textureImageView, nullptr);

	vkDestroyImage(device, textureImage, nullptr);
	vkFreeMemory(device, textureImageMemory, nullptr);

	vkDestroySampler(device, normalSampler, nullptr);
	vkDestroyImageView(device, normalImageView, nullptr);
	vkDestroyImage(device, normalImage, nullptr);
	vkFreeMemory(device, normalImageMemory, nullptr);

	vkDestroyPipeline(device, boxGraphicsPipeline, nullptr);
	vkDestroyPipeline(device, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(device, boxPipelineLayout, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, boxDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	vkDestroyBuffer(device, indexBuffer, nullptr);
	vkFreeMemory(device, indexBufferMemory, nullptr);

	vkDestroyBuffer(device, vertexBuffer, nullptr);
	vkFreeMemory(device, vertexBufferMemory, nullptr);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(device, inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(device, commandPool, nullptr);

	vkDestroyDevice(device, nullptr);

	vkDestroySurfaceKHR(instance_, surface, nullptr);
	vkDestroyInstance(instance_, nullptr);

	glfwDestroyWindow(window);
	glfwTerminate();
}

/** @brief F ????????????????????? camera.BeginSmoothFocus */
void VulkanRender::tryBeginCameraFocusOnPick()
{
	glm::vec3 focus{ 0.0f };
	float distance = 4.0f;
	if (mainModelSelected) {
		const glm::vec3 ext = modelLocalBoundsMax - modelLocalBoundsMin;
		if (glm::length(ext) < 1e-5f) {
			focus = mainModelPosition;
			distance = 5.0f;
		}
		else {
			focus = 0.5f * (modelLocalBoundsMin + modelLocalBoundsMax) + mainModelPosition;
			distance = glm::max(3.0f, glm::length(ext) * 1.75f);
		}
	}
	else if (pickedBoxEntityId != 0) {
		focus = getBoxPosition(pickedBoxEntityId);
		distance = 2.5f;
	}
	else {
		return;
	}
	camera.BeginSmoothFocus(focus, distance, 0.65f);
}

/** @brief ? WASD  F ?? VulkanRender.hpp */
void VulkanRender::processInput(GLFWwindow* w)
{
	static bool fKeyWasDown = false;
	const bool fKeyDown = glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS;
	const bool imguiWantsKb =
		ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
	if (!imguiWantsKb && fKeyDown && !fKeyWasDown) {
		auto* app = static_cast<VulkanRender*>(glfwGetWindowUserPointer(w));
		if (app) {
			app->tryBeginCameraFocusOnPick();
		}
	}
	fKeyWasDown = fKeyDown;

	if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS) {
		camera.speedZ = 1.0f;
	}
	else if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS) {
		camera.speedZ = -1.0f;
	}
	else {
		camera.speedZ = 0.0f;
	}
	if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS) {
		camera.speedX = -1.0f;
	}
	else if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS) {
		camera.speedX = 1.0f;
	}
	else {
		camera.speedX = 0.0f;
	}
}
