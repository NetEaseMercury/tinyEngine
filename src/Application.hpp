#pragma once
#include "VulkanContext.hpp"
#include "SwapChain.hpp"
#include "RenderPassManager.hpp"
#include "FramebufferManager.hpp"
#include "CommandManager.hpp"
#include "BufferManager.hpp"
#include "PipelineManager.hpp"
#include "DescriptorManager.hpp"
#include "MaterialManager.hpp"
#include "SceneManager.hpp"
#include "PickSystem.hpp"
#include "IMGUIManager.hpp"
#include "camera.hpp"
#include "VulkanTypes.hpp"
#include <glm/glm.hpp>

class Application {
public:
    Application();
    using RenderEntityId = uint64_t;

    void run();

    // Public state read/written by UIManager
    glm::vec3      mainModelPosition{ 0.f, 0.f, 0.f };
    bool           mainModelSelected  = false;
    RenderEntityId pickedBoxEntityId  = 0;
    MaterialId     selectedMaterialId = kInvalidMaterialId;

    // UIManager-facing API
    GLFWwindow*  getMainWindow()           const { return window_; }
    VkRenderPass getMainRenderPass()       const { return rpMgr_.getMainRenderPass(); }
    VkQueue      getGraphicsQueue()        const { return ctx_.getGraphicsQueue(); }
    uint32_t     getGraphicsQueueFamily()  const { return ctx_.getGraphicsQueueFamily(); }
    uint32_t     getSwapChainImageCount()  const { return swapChain_.getImageCount(); }

    glm::mat4 getSceneViewMatrix();
    glm::mat4 getSceneProjMatrixForImGuizmo();

    RenderEntityId addBox(const glm::vec3& pos);
    bool           removeBox(RenderEntityId id);
    glm::vec3      getBoxPosition(RenderEntityId id) const;
    void           setBoxPosition(RenderEntityId id, const glm::vec3& pos);

    // Material accessors for UIManager
    MaterialManager& getMaterialManager()            { return matMgr_; }
    SceneManager&    getSceneManager()               { return sceneMgr_; }

    void setModelMaterial(MaterialId id);
    void setBoxMaterial(RenderEntityId eid, MaterialId id);
    MaterialId getModelMaterial()               const { return sceneMgr_.getModelMaterialId(); }
    MaterialId getBoxMaterial(RenderEntityId e) const { return sceneMgr_.getBoxMaterialId(e); }

    // Thin wrappers so UIManager can drive MaterialManager without Vulkan context access
    MaterialId createMeshMaterial(const std::string& name,
                                  const std::string& albedoPath,
                                  const std::string& normalPath,
                                  const MaterialParams& params);
    MaterialId createBoxMaterial(const std::string& name, const MaterialParams& params);
    void       destroyMaterial(MaterialId id);
    void       setMaterialAlbedo(MaterialId id, const std::string& path);
    void       setMaterialNormal(MaterialId id, const std::string& path);

private:
    GLFWwindow* window_  = nullptr;
    bool        framebufferResized_ = false;
    uint32_t    currentFrame_ = 0;

    VulkanContext      ctx_;
    SwapChain          swapChain_;
    RenderPassManager  rpMgr_;
    FramebufferManager fbMgr_;
    CommandManager     cmdMgr_;
    BufferManager      bufMgr_;
    PipelineManager    pipeMgr_;
    DescriptorManager  descMgr_;
    MaterialManager    matMgr_;
    SceneManager       sceneMgr_;
    PickSystem         pickSys_;
    UIManager*         ui_  = nullptr;
    Camera             camera_;

    bool  firstMouse_       = true;
    bool  rightMouseDown_   = false;
    float lastX_            = WIDTH / 2.f;
    float lastY_            = HEIGHT / 2.f;

    void initGLFW();
    void initVulkan();
    void gameLoop();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer cb, uint32_t imageIndex);
    void recreateSwapChain();
    void processInput(GLFWwindow* w);
    void tryPickMainModel(float cx, float cy);
    void tryBeginCameraFocusOnPick();
    void cleanUp();

    static void framebufferResizeCallback(GLFWwindow* w, int, int);
    static void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods);
    static void mouseCallback(GLFWwindow* w, double xpos, double ypos);
};
