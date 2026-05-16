#include "Application.hpp"
#include "TinyEngineDebug.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

Application::Application()
    : camera_(glm::vec3(0.f, -4.f, 4.f), glm::radians(45.f), glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f))
{}

// ─── GLFW callback implementations ───────────────────────────────────────────

void Application::framebufferResizeCallback(GLFWwindow* w, int, int)
{
    auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(w));
    if (app) app->framebufferResized_ = true;
}

void Application::mouseButtonCallback(GLFWwindow* w, int button, int action, int mods)
{
    auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(w));
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        app->rightMouseDown_ = (action == GLFW_PRESS);
    }
    if (app && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
        if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) return;
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;
        double cx = 0.0, cy = 0.0;
        glfwGetCursorPos(w, &cx, &cy);
        app->tryPickMainModel(static_cast<float>(cx), static_cast<float>(cy));
    }
}

void Application::mouseCallback(GLFWwindow* w, double xpos, double ypos)
{
    auto* app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(w));
    if (!app || !app->rightMouseDown_) return;
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) return;

    if (app->firstMouse_) {
        app->lastX_ = static_cast<float>(xpos);
        app->lastY_ = static_cast<float>(ypos);
        app->firstMouse_ = false;
    }
    const float dx = static_cast<float>(xpos) - app->lastX_;
    const float dy = static_cast<float>(ypos) - app->lastY_;
    app->lastX_ = static_cast<float>(xpos);
    app->lastY_ = static_cast<float>(ypos);
    app->camera_.ProcessMouseMovement(dx, dy);
}

// ─── Public entry ─────────────────────────────────────────────────────────────

void Application::run()
{
    ui_ = new UIManager();
    tinyengine::debug::initRenderDoc();
    initGLFW();
    ui_->setModelDefaultPath();
    initVulkan();
    gameLoop();
}

// ─── Initialisation ───────────────────────────────────────────────────────────

void Application::initGLFW()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window_ = glfwCreateWindow(WIDTH, HEIGHT, "tinyEngine", nullptr, nullptr);
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    glfwSetCursorPosCallback(window_, mouseCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void Application::initVulkan()
{
    ctx_.init(window_);
    tinyengine::debug::initDebugUtils(ctx_.getInstance());

    cmdMgr_.create(ctx_);
    swapChain_.create(ctx_, window_);
    rpMgr_.create(ctx_, swapChain_);

    bufMgr_.init(ctx_, cmdMgr_);

    pipeMgr_.create(ctx_, rpMgr_,
                    ui_->vertexShaderPath, ui_->fragShaderPath,
                    ui_->boxVertShaderPath, ui_->boxFragShaderPath,
                    swapChain_.getExtent());

    fbMgr_.create(ctx_, swapChain_, rpMgr_);

    sceneMgr_.createCubeTemplate(bufMgr_);
    sceneMgr_.loadModel(ui_->modelPath, glm::vec3(0.f), bufMgr_);

    matMgr_.init(ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_,
                 swapChain_.getImageCount(), ui_->texturePath);
    sceneMgr_.setModelMaterialId(matMgr_.getDefaultMeshMaterialId());

    // Try to apply the JSON material asset to the main model.
    // On failure (file missing, parse error, texture missing) the default
    // mesh material remains in effect.
    {
        const MaterialId mid = matMgr_.loadMaterialFromAsset(
            "materials/mainmodel.ast", ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
        if (mid != kInvalidMaterialId) sceneMgr_.setModelMaterialId(mid);
    }

    descMgr_.create(ctx_, swapChain_, pipeMgr_, bufMgr_);

    ui_->setVulkanInstance(ctx_.getInstance(), nullptr);
    ui_->setPhysicalDevice(ctx_.getDevice(), ctx_.getPhysicalDevice());
    ui_->setVulkanRender(this);
    ui_->initIMGUI();

    // Dummy first frame to initialise ImGui draw data
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Render();

    pickSys_.create(ctx_, cmdMgr_);
    cmdMgr_.allocateCommandBuffers(ctx_, swapChain_.getImageCount());
    cmdMgr_.createSyncObjects(ctx_, swapChain_.getImageCount());
}

// ─── Main loop ────────────────────────────────────────────────────────────────

void Application::gameLoop()
{
    while (!glfwWindowShouldClose(window_)) {
        if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            std::cout << "Exit Game" << std::endl;
            vkDeviceWaitIdle(ctx_.getDevice());
            break;
        }

        glfwPollEvents();

        const float now       = static_cast<float>(glfwGetTime());
        static float prevTick = -1.0f;
        const float dt        = (prevTick < 0.f) ? 0.f : glm::max(0.f, now - prevTick);
        prevTick = now;

        processInput(window_);
        if (!camera_.IsSmoothFocusActive())
            camera_.UpdataCameraPosition();
        camera_.UpdateSmoothFocus(dt > 0.f ? dt : 1.f / 240.f);

        ui_->prepareFrame();
        drawFrame();

        camera_.SPEED = ui_->updateSpeed();
        if (ui_->refreshVulkanShader()) {
            recreateSwapChain();
        }
        ui_->setRefreshVulkanStatus(false);
    }

    ui_->cleanUp();
    cleanUp();
}

// ─── Draw frame ───────────────────────────────────────────────────────────────

void Application::drawFrame()
{
    vkWaitForFences(ctx_.getDevice(), 1, &cmdMgr_.getInFlightFence(currentFrame_), VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(ctx_.getDevice(), swapChain_.getSwapChain(),
                                            UINT64_MAX,
                                            cmdMgr_.getImageAvailableSemaphore(currentFrame_),
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapChain(); return; }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swap chain image!");

    const glm::mat4 view = camera_.GetViewMatrix();
    const VkExtent2D ext = swapChain_.getExtent();
    const glm::mat4 proj = [&]() {
        glm::mat4 p = glm::perspective(glm::radians(45.f),
                                       ext.width / static_cast<float>(ext.height),
                                       0.1f, 500.f);
        p[1][1] *= -1;
        return p;
    }();
    matMgr_.updateAllUBOs(imageIndex, view, proj);

    VkFence& imgFence = cmdMgr_.getImageInFlight(imageIndex);
    if (imgFence != VK_NULL_HANDLE)
        vkWaitForFences(ctx_.getDevice(), 1, &imgFence, VK_TRUE, UINT64_MAX);

    VkCommandBuffer cb = cmdMgr_.getCommandBuffer(imageIndex);
    recordCommandBuffer(cb, imageIndex);

    imgFence = cmdMgr_.getInFlightFence(currentFrame_);

    VkSemaphore waitSems[]   = { cmdMgr_.getImageAvailableSemaphore(currentFrame_) };
    VkSemaphore signalSems[] = { cmdMgr_.getRenderFinishedSemaphore(currentFrame_) };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo si{};
    si.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount   = 1; si.pWaitSemaphores   = waitSems;
    si.pWaitDstStageMask    = waitStages;
    si.commandBufferCount   = 1; si.pCommandBuffers   = &cb;
    si.signalSemaphoreCount = 1; si.pSignalSemaphores = signalSems;

    vkResetFences(ctx_.getDevice(), 1, &cmdMgr_.getInFlightFence(currentFrame_));
    if (vkQueueSubmit(ctx_.getGraphicsQueue(), 1, &si, cmdMgr_.getInFlightFence(currentFrame_)) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit draw command buffer!");

    VkSwapchainKHR sc = swapChain_.getSwapChain();
    VkPresentInfoKHR pi{};
    pi.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = signalSems;
    pi.swapchainCount     = 1; pi.pSwapchains     = &sc;
    pi.pImageIndices      = &imageIndex;

    result = vkQueuePresentKHR(ctx_.getPresentQueue(), &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Application::recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex)
{
    vkResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer!");

    std::array<VkClearValue, 2> clears{};
    if (ui_) {
        const ImVec4 cc = ui_->getClearColor();
        clears[0].color = { cc.x * cc.w, cc.y * cc.w, cc.z * cc.w, cc.w };
    } else {
        clears[0].color = { 0.f, 0.f, 0.f, 1.f };
    }
    clears[1].depthStencil = { 1.f, 0 };

    VkRenderPassBeginInfo rpi{};
    rpi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpi.renderPass        = rpMgr_.getMainRenderPass();
    rpi.framebuffer       = fbMgr_.getFramebuffer(imageIndex);
    rpi.renderArea.extent = swapChain_.getExtent();
    rpi.clearValueCount   = static_cast<uint32_t>(clears.size());
    rpi.pClearValues      = clears.data();

    TINYENGINE(cb, "Main Render Pass");
    vkCmdBeginRenderPass(cb, &rpi, VK_SUBPASS_CONTENTS_INLINE);

    // Main model — use its assigned material descriptor set
    if (sceneMgr_.getModelIndexCount() > 0) {
        TINYENGINE(cb, "Scene Geometry");
        const MaterialId meshMat = matMgr_.isValid(sceneMgr_.getModelMaterialId())
            ? sceneMgr_.getModelMaterialId()
            : matMgr_.getDefaultMeshMaterialId();

        VkPipeline meshPipe = matMgr_.getPipeline(meshMat, ctx_, pipeMgr_);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, meshPipe);
        VkBuffer vb = sceneMgr_.getVertexBuffer(); VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cb, sceneMgr_.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        VkDescriptorSet ds = matMgr_.getDescriptorSet(meshMat, imageIndex);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeMgr_.getMainPipelineLayout(), 0, 1, &ds, 0, nullptr);
        glm::mat4 model = glm::translate(glm::mat4(1.f), mainModelPosition);
        vkCmdPushConstants(cb, pipeMgr_.getMainPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);
        vkCmdDrawIndexed(cb, sceneMgr_.getModelIndexCount(), 1, 0, 0, 0);
    }

    // GPU-instanced boxes — all share the default box material (preserves instancing)
    if (sceneMgr_.getInstanceCount() > 0 && sceneMgr_.getInstanceBuffer() != VK_NULL_HANDLE) {
        TINYENGINE(cb, "Box Geometry");
        const MaterialId boxMat = matMgr_.getDefaultBoxMaterialId();
        VkPipeline boxPipe = matMgr_.getPipeline(boxMat, ctx_, pipeMgr_);
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, boxPipe);
        VkDescriptorSet ds = matMgr_.getDescriptorSet(boxMat, imageIndex);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeMgr_.getBoxPipelineLayout(), 0, 1, &ds, 0, nullptr);
        VkBuffer bufs[] = { sceneMgr_.getCubeVertexBuffer(), sceneMgr_.getInstanceBuffer() };
        VkDeviceSize offs[] = { 0, 0 };
        vkCmdBindVertexBuffers(cb, 0, 2, bufs, offs);
        vkCmdBindIndexBuffer(cb, sceneMgr_.getCubeIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cb, sceneMgr_.getCubeIndexCount(), sceneMgr_.getInstanceCount(), 0, 0, 0);
    }

    // ImGui
    if (ImGui::GetCurrentContext()) {
        ImDrawData* dd = ImGui::GetDrawData();
        if (dd && dd->Valid) {
            TINYENGINE(cb, "ImGui Overlay");
            ImGui_ImplVulkan_RenderDrawData(dd, cb);
        }
    }

    vkCmdEndRenderPass(cb);
    if (vkEndCommandBuffer(cb) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer!");
}

// ─── Swapchain recreation ─────────────────────────────────────────────────────

void Application::recreateSwapChain()
{
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    while (w == 0 || h == 0) {
        glfwGetFramebufferSize(window_, &w, &h);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle(ctx_.getDevice());

    pickSys_.destroy(ctx_, cmdMgr_);
    cmdMgr_.freeCommandBuffers(ctx_);
    descMgr_.destroy(ctx_);
    matMgr_.destroy(ctx_);
    fbMgr_.destroy(ctx_);
    pipeMgr_.destroyPipelines(ctx_);
    rpMgr_.destroy(ctx_);
    swapChain_.destroy(ctx_);

    swapChain_.create(ctx_, window_);
    rpMgr_.create(ctx_, swapChain_);

    if (ui_) ui_->reloadImGuiVulkanAfterSwapchainRecreate(this);

    pipeMgr_.recreate(ctx_, rpMgr_,
                      ui_->vertexShaderPath, ui_->fragShaderPath,
                      ui_->boxVertShaderPath, ui_->boxFragShaderPath,
                      swapChain_.getExtent());

    fbMgr_.create(ctx_, swapChain_, rpMgr_);

    sceneMgr_.destroyModelBuffers(ctx_);
    sceneMgr_.loadModel(ui_->modelPath, glm::vec3(0.f), bufMgr_);
    mainModelPosition  = glm::vec3(0.f);
    mainModelSelected  = false;
    pickedBoxEntityId  = 0;

    matMgr_.init(ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_,
                 swapChain_.getImageCount(), ui_->texturePath);
    sceneMgr_.setModelMaterialId(matMgr_.getDefaultMeshMaterialId());

    // Re-apply the asset material after swapchain recreate.
    {
        const MaterialId mid = matMgr_.loadMaterialFromAsset(
            "materials/mainmodel.ast", ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
        if (mid != kInvalidMaterialId) sceneMgr_.setModelMaterialId(mid);
    }

    descMgr_.create(ctx_, swapChain_, pipeMgr_, bufMgr_);
    pickSys_.create(ctx_, cmdMgr_);
    cmdMgr_.allocateCommandBuffers(ctx_, swapChain_.getImageCount());
}

// ─── Picking ──────────────────────────────────────────────────────────────────

void Application::tryPickMainModel(float cx, float cy)
{
    mainModelSelected = false;
    pickedBoxEntityId = 0;

    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window_, &winW, &winH);
    glfwGetFramebufferSize(window_, &fbW, &fbH);
    if (winW <= 0 || winH <= 0 || fbW <= 0 || fbH <= 0) return;

    const float fx = cx * static_cast<float>(fbW) / static_cast<float>(winW);
    const float fy = cy * static_cast<float>(fbH) / static_cast<float>(winH);
    const int maxX = static_cast<int>(swapChain_.getExtent().width)  - 1;
    const int maxY = static_cast<int>(swapChain_.getExtent().height) - 1;
    if (maxX < 0 || maxY < 0) return;
    const int px = std::max(0, std::min(static_cast<int>(fx), maxX));
    const int py = std::max(0, std::min(static_cast<int>(fy), maxY));

    // Update pick UBO slot 0 with current camera before pick pass
    const VkExtent2D ext = swapChain_.getExtent();
    const glm::mat4 view = camera_.GetViewMatrix();
    const glm::mat4 proj = [&]() {
        glm::mat4 p = glm::perspective(glm::radians(45.f),
                                       ext.width / static_cast<float>(ext.height),
                                       0.1f, 500.f);
        p[1][1] *= -1;
        return p;
    }();
    descMgr_.updateUniformBuffer(0, view, proj);

    const uint32_t id = pickSys_.runPick(ctx_, rpMgr_, fbMgr_, pipeMgr_,
                                          descMgr_.getBoxDescriptorSet(0),
                                          sceneMgr_, mainModelPosition, ext,
                                          static_cast<uint32_t>(px),
                                          static_cast<uint32_t>(py));

    if (id == SceneManager::kPickIdNone) return;
    if (id == SceneManager::kPickIdMainModel) {
        mainModelSelected  = true;
        selectedMaterialId = sceneMgr_.getModelMaterialId();
        return;
    }
    if (id >= SceneManager::kPickIdBoxBase) {
        const uint32_t idx = id - SceneManager::kPickIdBoxBase;
        const auto& ids = sceneMgr_.getBoxRangeEntityIds();
        if (idx < ids.size()) {
            pickedBoxEntityId  = ids[idx];
            selectedMaterialId = sceneMgr_.getBoxMaterialId(pickedBoxEntityId);
        }
    }
}

void Application::tryBeginCameraFocusOnPick()
{
    glm::vec3 focus(0.f);
    float distance = 4.f;
    if (mainModelSelected) {
        const glm::vec3 ext = sceneMgr_.getModelBoundsMax() - sceneMgr_.getModelBoundsMin();
        if (glm::length(ext) < 1e-5f) { focus = mainModelPosition; distance = 5.f; }
        else {
            focus    = 0.5f * (sceneMgr_.getModelBoundsMin() + sceneMgr_.getModelBoundsMax())
                       + mainModelPosition;
            distance = glm::max(3.f, glm::length(ext) * 1.75f);
        }
    } else if (pickedBoxEntityId != 0) {
        focus    = sceneMgr_.getBoxPosition(pickedBoxEntityId);
        distance = 2.5f;
    } else {
        return;
    }
    camera_.BeginSmoothFocus(focus, distance, 0.65f);
}

// ─── Material API ─────────────────────────────────────────────────────────────

void Application::setModelMaterial(MaterialId id)
{
    if (!matMgr_.isValid(id)) return;
    sceneMgr_.setModelMaterialId(id);
    if (mainModelSelected) selectedMaterialId = id;
}

void Application::setBoxMaterial(RenderEntityId eid, MaterialId id)
{
    if (!matMgr_.isValid(id)) return;
    sceneMgr_.setBoxMaterialId(eid, id);
    if (pickedBoxEntityId == eid) selectedMaterialId = id;
}

bool Application::loadAndApplyMaterialAsset(const std::string& astRelPath)
{
    const MaterialId mid = matMgr_.loadMaterialFromAsset(
        astRelPath, ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
    if (mid == kInvalidMaterialId) return false;
    sceneMgr_.setModelMaterialId(mid);
    if (mainModelSelected) selectedMaterialId = mid;
    return true;
}

MaterialId Application::createMeshMaterial(const std::string& name,
                                           const std::string& albedoPath,
                                           const std::string& normalPath,
                                           const MaterialParams& params)
{
    return matMgr_.createMeshMaterial(name, albedoPath, normalPath, params,
                                      ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
}

MaterialId Application::createBoxMaterial(const std::string& name, const MaterialParams& params)
{
    return matMgr_.createBoxMaterial(name, params, ctx_, bufMgr_, pipeMgr_);
}

void Application::destroyMaterial(MaterialId id)
{
    matMgr_.destroyMaterial(id, ctx_);
}

void Application::setMaterialAlbedo(MaterialId id, const std::string& path)
{
    matMgr_.setAlbedoPath(id, path, ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
}

void Application::setMaterialNormal(MaterialId id, const std::string& path)
{
    matMgr_.setNormalPath(id, path, ctx_, cmdMgr_, bufMgr_, fbMgr_, pipeMgr_);
}

// ─── Input ────────────────────────────────────────────────────────────────────

void Application::processInput(GLFWwindow* w)
{
    static bool fKeyWasDown = false;
    const bool fKeyDown = glfwGetKey(w, GLFW_KEY_F) == GLFW_PRESS;
    const bool imguiKb  = ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard;
    if (!imguiKb && fKeyDown && !fKeyWasDown)
        tryBeginCameraFocusOnPick();
    fKeyWasDown = fKeyDown;

    camera_.speedZ = (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)  ?  1.f
                   : (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)  ? -1.f : 0.f;
    camera_.speedX = (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)  ? -1.f
                   : (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)  ?  1.f : 0.f;
}

// ─── UIManager delegation ─────────────────────────────────────────────────────

glm::mat4 Application::getSceneViewMatrix()
{
    return camera_.GetViewMatrix();
}

glm::mat4 Application::getSceneProjMatrixForImGuizmo()
{
    const VkExtent2D ext = swapChain_.getExtent();
    return glm::perspective(glm::radians(45.f),
                            ext.width / static_cast<float>(ext.height),
                            0.1f, 500.f);
}

Application::RenderEntityId Application::addBox(const glm::vec3& pos)
{
    RenderEntityId eid = sceneMgr_.addBox(pos, ctx_, bufMgr_);
    sceneMgr_.setBoxMaterialId(eid, matMgr_.getDefaultBoxMaterialId());
    return eid;
}

bool Application::removeBox(RenderEntityId id)
{
    return sceneMgr_.removeBox(id, ctx_, bufMgr_);
}

glm::vec3 Application::getBoxPosition(RenderEntityId id) const
{
    return sceneMgr_.getBoxPosition(id);
}

void Application::setBoxPosition(RenderEntityId id, const glm::vec3& pos)
{
    sceneMgr_.setBoxPosition(id, pos, ctx_, bufMgr_);
}

// ─── Cleanup ──────────────────────────────────────────────────────────────────

void Application::cleanUp()
{
    vkDeviceWaitIdle(ctx_.getDevice());

    pickSys_.destroy(ctx_, cmdMgr_);
    sceneMgr_.destroy(ctx_);
    matMgr_.destroy(ctx_);
    descMgr_.destroy(ctx_);
    pipeMgr_.destroy(ctx_);
    fbMgr_.destroy(ctx_);
    cmdMgr_.destroy(ctx_);
    rpMgr_.destroy(ctx_);
    swapChain_.destroy(ctx_);
    ctx_.destroy();

    glfwDestroyWindow(window_);
    glfwTerminate();
    delete ui_;
    ui_ = nullptr;
}
