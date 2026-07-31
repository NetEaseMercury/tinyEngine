#pragma once

#include "SceneManager.hpp"
#include "MaterialManager.hpp"
#include "camera.hpp"
#include "Transform.hpp"
#include "VulkanTypes.hpp"
#include <glm/glm.hpp>
#include <string>

class Renderer;

/**
 * @class EngineRuntime
 * @brief Runtime layer: pure state and business logic for scene/materials/camera/
 *        selection. Owns no Vulkan Manager.
 *
 * Split out of the original Application god class. Every operation that touches
 * the GPU goes through the Renderer passed as a parameter (and its Manager
 * accessors). The Runtime itself only holds:
 *   - SceneManager  (model/box/SubMesh/material bindings)
 *   - MaterialManager (material params/textures/UBO/descriptors)
 *   - Camera + mainModelTransform + selection state
 * Calling convention: every method must be called on the render thread
 * (guaranteed by the Presenter).
 */
class EngineRuntime {
public:
    EngineRuntime();

    // ── Lifecycle ────────────────────────────────────────────────────
    /** @brief Build the initial scene: box template, default model, default material + mainmodel.ast + glTF auto .ast. */
    void initScene(Renderer& r, const std::string& modelPath, const std::string& defaultTexturePath);
    /** @brief Reload the model and all materials after swapchain recreation (keeps the current model, no fallback to the startup model). */
    void onSwapchainRecreate(Renderer& r);
    /** @brief Release scene and material GPU resources (called before the render thread exits). */
    void destroy(Renderer& r);

    // ── Scene / model ─────────────────────────────────────────────────
    /** @brief Load a .ast and apply it to the main model; switches the model too when the .ast carries a model field. */
    bool loadAndApplyMaterialAsset(Renderer& r, const std::string& astRelPath);
    /** @brief Switch the main model directly (res-relative path, e.g. "models/viking_room.obj"). */
    bool loadModelFromRes(Renderer& r, const std::string& relPath);

    RenderEntityId addBox(Renderer& r, RenderEntityId id, const glm::vec3& pos);
    bool           removeBox(Renderer& r, RenderEntityId id);
    glm::vec3      getBoxPosition(RenderEntityId id) const;
    void           setBoxPosition(Renderer& r, RenderEntityId id, const glm::vec3& pos);

    // ── Materials ─────────────────────────────────────────────────────
    void setModelMaterial(MaterialId id);
    void setBoxMaterial(RenderEntityId eid, MaterialId id);
    void setMaterialParams(MaterialId id, const MaterialParams& params);

    // ── Picking / selection / camera focus ───────────────────────────
    /** @brief Perform GPU picking at window coordinates (cx,cy) and update the selection state. */
    void tryPickMainModel(Renderer& r, GLFWwindow* window, float cx, float cy);
    /** @brief F key: smoothly focus the camera on the current selection. */
    void tryBeginCameraFocusOnPick();
    /** @brief Set selection from the editor: what 0=clear, 1=main model, 2=box(boxId). */
    void select(int what, RenderEntityId boxId);

    /** @brief The first material of the main model that actually renders (slot-0 material for glTF, whole-model material for obj). */
    MaterialId firstRenderedModelMaterialId() const;

    // ── State access ──────────────────────────────────────────────────
    Camera&          camera()   { return camera_; }
    SceneManager&    scene()    { return sceneMgr_; }
    MaterialManager& materials(){ return matMgr_; }
    const std::string& currentModelPath()   const { return currentModelPath_; }
    const std::string& defaultTexturePath() const { return defaultTexturePath_; }

    ObjectTransform mainModelTransform;             ///< Main model TRS
    bool           mainModelSelected  = false;
    RenderEntityId pickedBoxEntityId  = 0;
    MaterialId     selectedMaterialId = kInvalidMaterialId;
    float          clearColor[4] = { 0.45f, 0.55f, 0.60f, 1.0f };

private:
    /** @brief Load and bind the per-slot .ast files auto-dumped from glTF one by one. */
    void applyAutoAstMaterials(Renderer& r);
    /** @brief Apply materials/mainmodel.ast (keeps the default material on failure). */
    void applyMainModelAst(Renderer& r);

    SceneManager    sceneMgr_;
    MaterialManager matMgr_;
    Camera          camera_;
    std::string     currentModelPath_;
    std::string     defaultTexturePath_;
};
