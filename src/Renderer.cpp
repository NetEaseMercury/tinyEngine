#include "Renderer.hpp"
#include "EngineRuntime.hpp"
#include "TinyEngineDebug.hpp"
#include <array>
#include <stdexcept>
#include <glm/gtc/matrix_transform.hpp>

// ─── Initialisation ───────────────────────────────────────────────────────────

void Renderer::initCore(GLFWwindow* window,
                        const std::string& vertSpv, const std::string& fragSpv,
                        const std::string& boxVertSpv, const std::string& boxFragSpv)
{
    window_     = window;
    vertSpv_    = vertSpv;
    fragSpv_    = fragSpv;
    boxVertSpv_ = boxVertSpv;
    boxFragSpv_ = boxFragSpv;

    ctx_.init(window_);
    tinyengine::debug::initDebugUtils(ctx_.getInstance());

    cmdMgr_.create(ctx_);
    swapChain_.create(ctx_, window_);
    rpMgr_.create(ctx_, swapChain_);
    bufMgr_.init(ctx_, cmdMgr_);

    pipeMgr_.create(ctx_, rpMgr_,
                    vertSpv_, fragSpv_,
                    boxVertSpv_, boxFragSpv_,
                    swapChain_.getExtent());

    fbMgr_.create(ctx_, swapChain_, rpMgr_);
}

void Renderer::initFrameResources()
{
    descMgr_.create(ctx_, swapChain_, pipeMgr_, bufMgr_);
    pickSys_.create(ctx_, cmdMgr_);
    cmdMgr_.allocateCommandBuffers(ctx_, swapChain_.getImageCount());
    cmdMgr_.createSyncObjects(ctx_, swapChain_.getImageCount());
}

// ─── Draw frame ───────────────────────────────────────────────────────────────

void Renderer::drawFrame(EngineRuntime& runtime)
{
    vkWaitForFences(ctx_.getDevice(), 1, &cmdMgr_.getInFlightFence(currentFrame_), VK_TRUE, UINT64_MAX);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(ctx_.getDevice(), swapChain_.getSwapChain(),
                                            UINT64_MAX,
                                            cmdMgr_.getImageAvailableSemaphore(currentFrame_),
                                            VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapChain(runtime); return; }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("Failed to acquire swap chain image!");

    const glm::mat4 view = runtime.camera().GetViewMatrix();
    const glm::mat4 proj = runtime.camera().GetProjectionMatrix();
    runtime.materials().updateAllUBOs(imageIndex, view, proj);

    VkFence& imgFence = cmdMgr_.getImageInFlight(imageIndex);
    if (imgFence != VK_NULL_HANDLE)
        vkWaitForFences(ctx_.getDevice(), 1, &imgFence, VK_TRUE, UINT64_MAX);

    VkCommandBuffer cb = cmdMgr_.getCommandBuffer(imageIndex);
    recordCommandBuffer(cb, imageIndex, runtime);

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
        recreateSwapChain(runtime);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image!");
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, EngineRuntime& runtime)
{
    SceneManager&    sceneMgr_ = runtime.scene();
    MaterialManager& matMgr_   = runtime.materials();

    vkResetCommandBuffer(cb, 0);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cb, &bi) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer!");

    std::array<VkClearValue, 2> clears{};
    const float* cc = runtime.clearColor;
    clears[0].color = { cc[0] * cc[3], cc[1] * cc[3], cc[2] * cc[3], cc[3] };
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

    // Main model — iterate SubMeshes and switch material/descriptor set per entry.
    if (sceneMgr_.getModelIndexCount() > 0) {
        TINYENGINE(cb, "Scene Geometry");
        const MaterialId fallbackMat = matMgr_.isValid(sceneMgr_.getModelMaterialId())
            ? sceneMgr_.getModelMaterialId()
            : matMgr_.getDefaultMeshMaterialId();

        // Bind shared vertex/index buffer once; SubMeshes only differ in offset/count.
        VkBuffer vb = sceneMgr_.getVertexBuffer(); VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
        vkCmdBindIndexBuffer(cb, sceneMgr_.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

        PushConstants push{ runtime.mainModelTransform.GetModelMatrix(),
                            runtime.mainModelTransform.GetNormalMatrix() };
        vkCmdPushConstants(cb, pipeMgr_.getMainPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push);

        const auto& subs = sceneMgr_.getModelSubMeshes();
        // Track last-bound pipeline/desc set to skip redundant binds.
        VkPipeline lastPipe = VK_NULL_HANDLE;
        VkDescriptorSet lastDs = VK_NULL_HANDLE;

        auto drawSpan = [&](uint32_t indexOffset, uint32_t indexCount, MaterialId mat) {
            if (indexCount == 0) return;
            const MaterialId useMat = matMgr_.isValid(mat) ? mat : fallbackMat;
            VkPipeline pipe = matMgr_.getPipeline(useMat, ctx_, pipeMgr_);
            if (pipe != lastPipe) {
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
                lastPipe = pipe;
            }
            VkDescriptorSet ds = matMgr_.getDescriptorSet(useMat, imageIndex);
            if (ds != lastDs) {
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                        pipeMgr_.getMainPipelineLayout(), 0, 1, &ds, 0, nullptr);
                lastDs = ds;
            }
            vkCmdDrawIndexed(cb, indexCount, 1, indexOffset, 0, 0);
        };

        if (subs.empty()) {
            // Backwards-compat path: no SubMesh table -> single draw.
            drawSpan(0, sceneMgr_.getModelIndexCount(), fallbackMat);
        } else {
            for (const auto& sm : subs) {
                const MaterialId mat = sceneMgr_.getModelSubMeshMaterialId(sm.materialSlot);
                drawSpan(sm.indexOffset, sm.indexCount, mat);
            }
        }
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

    // ── Selection highlight outline (two stencil stages) + gizmo axes ────────
    const bool selModel = runtime.mainModelSelected && sceneMgr_.getModelIndexCount() > 0;
    const bool selBox   = !selModel && runtime.pickedBoxEntityId != 0 &&
                          sceneMgr_.getBoxes().count(runtime.pickedBoxEntityId) > 0;

    if (selModel || selBox) {
        const glm::mat4 viewProj = runtime.camera().GetProjectionMatrix() *
                                   runtime.camera().GetViewMatrix();
        const glm::vec3 anchor = selModel ? runtime.mainModelTransform.position
                                          : runtime.getBoxPosition(runtime.pickedBoxEntityId);
        const float dist = glm::length(runtime.camera().Position - anchor);

        // Redraw the selected object: in the mark stage thickness=0 and only the
        // stencil is written; in the outline stage the inflated shell is drawn.
        auto drawSelected = [&](VkPipeline pipe, float thickness, const glm::vec4& color) {
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
            UtilityPushConstants u{};
            u.viewProj  = viewProj;
            u.color     = color;
            u.thickness = thickness;
            VkDeviceSize off = 0;
            if (selModel) {
                u.model = runtime.mainModelTransform.GetModelMatrix();
                vkCmdPushConstants(cb, pipeMgr_.getUtilityPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(u), &u);
                VkBuffer vb = sceneMgr_.getVertexBuffer();
                vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
                vkCmdBindIndexBuffer(cb, sceneMgr_.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
                const auto& subs = sceneMgr_.getModelSubMeshes();
                if (subs.empty()) {
                    vkCmdDrawIndexed(cb, sceneMgr_.getModelIndexCount(), 1, 0, 0, 0);
                } else {
                    for (const auto& sm : subs)
                        if (sm.indexCount > 0)
                            vkCmdDrawIndexed(cb, sm.indexCount, 1, sm.indexOffset, 0, 0);
                }
            } else {
                u.model = glm::translate(glm::mat4(1.0f), anchor);
                vkCmdPushConstants(cb, pipeMgr_.getUtilityPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(u), &u);
                VkBuffer vb = sceneMgr_.getCubeVertexBuffer();
                vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
                vkCmdBindIndexBuffer(cb, sceneMgr_.getCubeIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cb, sceneMgr_.getCubeIndexCount(), 1, 0, 0, 0);
            }
        };

        const glm::vec4 outlineColor{ 1.0f, 0.62f, 0.05f, 1.0f }; // UE-style orange
        if (pipeMgr_.getMarkPipeline() != VK_NULL_HANDLE &&
            pipeMgr_.getOutlinePipeline() != VK_NULL_HANDLE) {
            TINYENGINE(cb, "Selection Outline");
            // Thickness adapts to camera distance, keeping a similar on-screen pixel
            // width both near and far
            const float thick = glm::clamp(dist * 0.0035f, 0.008f, 0.2f);
            drawSelected(pipeMgr_.getMarkPipeline(), 0.0f, outlineColor);
            drawSelected(pipeMgr_.getOutlinePipeline(), thick, outlineColor);
        }

        if (pipeMgr_.getGizmoPipeline() != VK_NULL_HANDLE) {
            TINYENGINE(cb, "Selection Gizmo");
            // Roughly constant on-screen size: axis length scales with camera distance
            const float len   = dist * 0.28f;
            const float thick = len * 0.05f;
            const struct { glm::mat4 rot; glm::vec4 color; } axes[3] = {
                { glm::mat4(1.0f),                                              { 0.92f, 0.20f, 0.22f, 1.0f } }, // X red
                { glm::rotate(glm::mat4(1.0f), glm::radians(90.0f),  { 0.f, 0.f, 1.f }), { 0.30f, 0.85f, 0.30f, 1.0f } }, // Y green
                { glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), { 0.f, 1.f, 0.f }), { 0.25f, 0.45f, 0.95f, 1.0f } }, // Z blue
            };
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeMgr_.getGizmoPipeline());
            VkDeviceSize off = 0;
            VkBuffer vb = sceneMgr_.getCubeVertexBuffer();
            vkCmdBindVertexBuffers(cb, 0, 1, &vb, &off);
            vkCmdBindIndexBuffer(cb, sceneMgr_.getCubeIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
            for (const auto& ax : axes) {
                UtilityPushConstants u{};
                u.viewProj  = viewProj;
                u.color     = ax.color;
                u.thickness = 0.0f;
                // Unit cube ([-0.5,0.5]) → stretch along X into an axis bar, translate to
                // 0..len, then rotate into place
                u.model = glm::translate(glm::mat4(1.0f), anchor) * ax.rot *
                          glm::translate(glm::mat4(1.0f), { len * 0.5f, 0.f, 0.f }) *
                          glm::scale(glm::mat4(1.0f), { len, thick, thick });
                vkCmdPushConstants(cb, pipeMgr_.getUtilityPipelineLayout(),
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(u), &u);
                vkCmdDrawIndexed(cb, sceneMgr_.getCubeIndexCount(), 1, 0, 0, 0);
            }
        }
    }

    vkCmdEndRenderPass(cb);
    if (vkEndCommandBuffer(cb) != VK_SUCCESS)
        throw std::runtime_error("Failed to record command buffer!");
}

// ─── Swapchain recreation ─────────────────────────────────────────────────────

void Renderer::recreateSwapChain(EngineRuntime& runtime)
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
    runtime.materials().destroy(ctx_);
    fbMgr_.destroy(ctx_);
    pipeMgr_.destroyPipelines(ctx_);
    rpMgr_.destroy(ctx_);
    swapChain_.destroy(ctx_);

    swapChain_.create(ctx_, window_);
    rpMgr_.create(ctx_, swapChain_);

    // Sync camera aspect ratio after swapchain recreate.
    {
        const VkExtent2D ext = swapChain_.getExtent();
        runtime.camera().SetAspectRatio(static_cast<float>(ext.width), static_cast<float>(ext.height));
    }

    pipeMgr_.recreate(ctx_, rpMgr_,
                      vertSpv_, fragSpv_,
                      boxVertSpv_, boxFragSpv_,
                      swapChain_.getExtent());

    fbMgr_.create(ctx_, swapChain_, rpMgr_);

    // Reload model + full material system against the new swapchain.
    runtime.onSwapchainRecreate(*this);

    descMgr_.create(ctx_, swapChain_, pipeMgr_, bufMgr_);
    pickSys_.create(ctx_, cmdMgr_);
    cmdMgr_.allocateCommandBuffers(ctx_, swapChain_.getImageCount());
}

// ─── Cleanup ──────────────────────────────────────────────────────────────────

void Renderer::destroy()
{
    vkDeviceWaitIdle(ctx_.getDevice());

    pickSys_.destroy(ctx_, cmdMgr_);
    descMgr_.destroy(ctx_);
    pipeMgr_.destroy(ctx_);
    fbMgr_.destroy(ctx_);
    cmdMgr_.destroy(ctx_);
    rpMgr_.destroy(ctx_);
    swapChain_.destroy(ctx_);
    ctx_.destroy();
}
