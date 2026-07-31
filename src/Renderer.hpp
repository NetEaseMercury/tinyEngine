#pragma once

#include "VulkanContext.hpp"
#include "SwapChain.hpp"
#include "RenderPassManager.hpp"
#include "FramebufferManager.hpp"
#include "CommandManager.hpp"
#include "BufferManager.hpp"
#include "PipelineManager.hpp"
#include "DescriptorManager.hpp"
#include "PickSystem.hpp"
#include "VulkanTypes.hpp"
#include <string>

class EngineRuntime;

/**
 * @class Renderer
 * @brief Render layer: the Vulkan rendering core. Owns all Vulkan Managers and
 *        handles init, per-frame rendering (drawFrame), swapchain recreation
 *        and teardown.
 *
 * Split out of the original Application god class. The Renderer knows nothing
 * about window creation, input, or the Editor's existence — the window handle
 * is passed in from outside (Presenter); scene/material data is read through
 * the EngineRuntime& parameter of drawFrame. Calling convention: every method
 * must be called on the render thread.
 */
class Renderer {
public:
    /** @brief Phase 1: Vulkan core (context/swapchain/renderpass/pipeline/framebuffer). */
    void initCore(GLFWwindow* window,
                  const std::string& vertSpv, const std::string& fragSpv,
                  const std::string& boxVertSpv, const std::string& boxFragSpv);
    /** @brief Phase 2: descriptors/picking/command buffers/sync objects (call after the Runtime scene is built). */
    void initFrameResources();
    /** @brief Render one frame (UBO update, command recording, submit and present). */
    void drawFrame(EngineRuntime& runtime);
    /** @brief Swapchain recreation (on resize/OUT_OF_DATE); coordinates with the Runtime to reload model and materials. */
    void recreateSwapChain(EngineRuntime& runtime);
    /** @brief Release all Vulkan resources except scene/materials (those are released first by Runtime::destroy). */
    void destroy();

    void setFramebufferResized() { framebufferResized_ = true; }

    // ── Manager accessors (for Runtime business logic to orchestrate GPU ops) ──
    VulkanContext&      ctx()       { return ctx_; }
    SwapChain&          swapChain() { return swapChain_; }
    RenderPassManager&  rpMgr()     { return rpMgr_; }
    FramebufferManager& fbMgr()     { return fbMgr_; }
    CommandManager&     cmdMgr()    { return cmdMgr_; }
    BufferManager&      bufMgr()    { return bufMgr_; }
    PipelineManager&    pipeMgr()   { return pipeMgr_; }
    DescriptorManager&  descMgr()   { return descMgr_; }
    PickSystem&         pickSys()   { return pickSys_; }

private:
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex, EngineRuntime& runtime);

    GLFWwindow* window_ = nullptr;

    VulkanContext      ctx_;
    SwapChain          swapChain_;
    RenderPassManager  rpMgr_;
    FramebufferManager fbMgr_;
    CommandManager     cmdMgr_;
    BufferManager      bufMgr_;
    PipelineManager    pipeMgr_;
    DescriptorManager  descMgr_;
    PickSystem         pickSys_;

    std::string vertSpv_, fragSpv_, boxVertSpv_, boxFragSpv_;

    uint32_t currentFrame_      = 0;
    bool     framebufferResized_ = false;
};
