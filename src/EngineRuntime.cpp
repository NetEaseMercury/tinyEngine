#include "EngineRuntime.hpp"
#include "Renderer.hpp"
#include "MaterialAssetLoader.hpp"
#include "EnginePaths.hpp"
#include <algorithm>
#include <iostream>

EngineRuntime::EngineRuntime()
    : camera_(glm::vec3(0.f, -4.f, 4.f), glm::radians(45.f), 0.0f, glm::vec3(0.f, 1.f, 0.f))
{}

// ─── Lifecycle ────────────────────────────────────────────────────────────────

void EngineRuntime::initScene(Renderer& r, const std::string& modelPath,
                              const std::string& defaultTexturePath)
{
    currentModelPath_   = modelPath;
    defaultTexturePath_ = defaultTexturePath;

    sceneMgr_.createCubeTemplate(r.bufMgr());
    sceneMgr_.loadModel(currentModelPath_, glm::vec3(0.f), r.bufMgr());

    matMgr_.init(r.ctx(), r.cmdMgr(), r.bufMgr(), r.fbMgr(), r.pipeMgr(),
                 r.swapChain().getImageCount(), defaultTexturePath_);
    sceneMgr_.setModelMaterialId(matMgr_.getDefaultMeshMaterialId());

    applyMainModelAst(r);
    applyAutoAstMaterials(r);
}

void EngineRuntime::onSwapchainRecreate(Renderer& r)
{
    sceneMgr_.destroyModelBuffers(r.ctx());
    sceneMgr_.loadModel(currentModelPath_, glm::vec3(0.f), r.bufMgr());
    mainModelTransform = ObjectTransform{};
    mainModelSelected  = false;
    pickedBoxEntityId  = 0;

    matMgr_.init(r.ctx(), r.cmdMgr(), r.bufMgr(), r.fbMgr(), r.pipeMgr(),
                 r.swapChain().getImageCount(), defaultTexturePath_);
    sceneMgr_.setModelMaterialId(matMgr_.getDefaultMeshMaterialId());

    applyMainModelAst(r);
    applyAutoAstMaterials(r);
}

void EngineRuntime::destroy(Renderer& r)
{
    sceneMgr_.destroy(r.ctx());
    matMgr_.destroy(r.ctx());
}

void EngineRuntime::applyMainModelAst(Renderer& r)
{
    const MaterialId mid = matMgr_.loadMaterialFromAsset(
        "materials/mainmodel.ast", r.ctx(), r.cmdMgr(), r.bufMgr(), r.fbMgr(), r.pipeMgr());
    if (mid != kInvalidMaterialId) sceneMgr_.setModelMaterialId(mid);
}

void EngineRuntime::applyAutoAstMaterials(Renderer& r)
{
    const auto& autoPaths = sceneMgr_.getModelAutoAstPaths();
    for (int slot = 0; slot < static_cast<int>(autoPaths.size()); ++slot) {
        if (autoPaths[slot].empty()) continue;
        const MaterialId mid = matMgr_.loadMaterialFromAsset(
            autoPaths[slot], r.ctx(), r.cmdMgr(), r.bufMgr(), r.fbMgr(), r.pipeMgr());
        if (mid != kInvalidMaterialId)
            sceneMgr_.setModelSubMeshMaterialId(slot, mid);
    }
}

// ─── Scene / model ────────────────────────────────────────────────────────────

bool EngineRuntime::loadModelFromRes(Renderer& r, const std::string& relPath)
{
    const std::string fullPath = te::paths::resolve(relPath);
    vkDeviceWaitIdle(r.ctx().getDevice());
    sceneMgr_.destroyModelBuffers(r.ctx());
    try {
        sceneMgr_.loadModel(fullPath, mainModelTransform.position, r.bufMgr());
    } catch (const std::exception& ex) {
        std::cerr << "[Runtime] load model failed (" << fullPath << "): " << ex.what() << "\n";
        return false;
    }
    currentModelPath_ = fullPath;
    sceneMgr_.setModelMaterialId(matMgr_.getDefaultMeshMaterialId());
    applyAutoAstMaterials(r);
    if (mainModelSelected) selectedMaterialId = firstRenderedModelMaterialId();
    return true;
}

RenderEntityId EngineRuntime::addBox(Renderer& r, RenderEntityId id, const glm::vec3& pos)
{
    RenderEntityId eid = sceneMgr_.addBoxWithId(id, pos, r.ctx(), r.bufMgr());
    sceneMgr_.setBoxMaterialId(eid, matMgr_.getDefaultBoxMaterialId());
    return eid;
}

bool EngineRuntime::removeBox(Renderer& r, RenderEntityId id)
{
    if (pickedBoxEntityId == id) {
        pickedBoxEntityId  = 0;
        selectedMaterialId = kInvalidMaterialId;
    }
    return sceneMgr_.removeBox(id, r.ctx(), r.bufMgr());
}

glm::vec3 EngineRuntime::getBoxPosition(RenderEntityId id) const
{
    return sceneMgr_.getBoxPosition(id);
}

void EngineRuntime::setBoxPosition(Renderer& r, RenderEntityId id, const glm::vec3& pos)
{
    sceneMgr_.setBoxPosition(id, pos, r.ctx(), r.bufMgr());
}

// ─── Materials ────────────────────────────────────────────────────────────────

void EngineRuntime::setModelMaterial(MaterialId id)
{
    if (!matMgr_.isValid(id)) return;
    sceneMgr_.setModelMaterialId(id);
    if (mainModelSelected) selectedMaterialId = id;
}

void EngineRuntime::setBoxMaterial(RenderEntityId eid, MaterialId id)
{
    if (!matMgr_.isValid(id)) return;
    sceneMgr_.setBoxMaterialId(eid, id);
    if (pickedBoxEntityId == eid) selectedMaterialId = id;
}

void EngineRuntime::setMaterialParams(MaterialId id, const MaterialParams& params)
{
    if (!matMgr_.isValid(id)) return;
    matMgr_.setParams(id, params);
}

bool EngineRuntime::loadAndApplyMaterialAsset(Renderer& r, const std::string& astRelPath)
{
    // Peek the asset first so we can do an optional mesh swap together with
    // the material change. MaterialAssetLoader::load only touches the JSON file
    // so the second internal load inside MaterialManager is cheap.
    MaterialAssetDesc peeked;
    const bool peekOk = MaterialAssetLoader::load(astRelPath, peeked, nullptr);

    const MaterialId mid = matMgr_.loadMaterialFromAsset(
        astRelPath, r.ctx(), r.cmdMgr(), r.bufMgr(), r.fbMgr(), r.pipeMgr());
    if (mid == kInvalidMaterialId) return false;

    // Optional mesh swap: .ast may carry a "model" field. Wait the GPU idle
    // before destroying the old vertex/index buffers since the in-flight
    // command buffers may still reference them.
    bool modelSwapped = false;
    if (peekOk && !peeked.modelPath.empty()) {
        vkDeviceWaitIdle(r.ctx().getDevice());
        sceneMgr_.destroyModelBuffers(r.ctx());
        try {
            sceneMgr_.loadModel(peeked.modelPath, mainModelTransform.position, r.bufMgr());
            modelSwapped = true;
            currentModelPath_ = peeked.modelPath;
        } catch (const std::exception& ex) {
            std::cerr << "[MaterialAsset] model swap failed (" << peeked.modelPath
                      << "): " << ex.what() << "\n";
        }
    }

    sceneMgr_.setModelMaterialId(mid);

    // If the swapped-in model was a glTF, it will have dumped per-submesh .ast
    // files into modelAutoAstPaths_. Load and bind each one now so materials
    // are applied immediately without requiring a swapchain recreate.
    if (modelSwapped)
        applyAutoAstMaterials(r);

    if (mainModelSelected) selectedMaterialId = firstRenderedModelMaterialId();
    return true;
}

// ─── Picking / selection / camera focus ───────────────────────────────────────

MaterialId EngineRuntime::firstRenderedModelMaterialId() const
{
    const auto& subs = sceneMgr_.getModelSubMeshes();
    for (const auto& sm : subs) {
        if (sm.materialSlot < 0) continue;
        const MaterialId id = sceneMgr_.getModelSubMeshMaterialId(sm.materialSlot);
        if (matMgr_.isValid(id)) return id;
    }
    return sceneMgr_.getModelMaterialId();
}

void EngineRuntime::tryPickMainModel(Renderer& r, GLFWwindow* window, float cx, float cy)
{
    mainModelSelected = false;
    pickedBoxEntityId = 0;

    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (winW <= 0 || winH <= 0 || fbW <= 0 || fbH <= 0) return;

    const float fx = cx * static_cast<float>(fbW) / static_cast<float>(winW);
    const float fy = cy * static_cast<float>(fbH) / static_cast<float>(winH);
    const int maxX = static_cast<int>(r.swapChain().getExtent().width)  - 1;
    const int maxY = static_cast<int>(r.swapChain().getExtent().height) - 1;
    if (maxX < 0 || maxY < 0) return;
    const int px = std::max(0, std::min(static_cast<int>(fx), maxX));
    const int py = std::max(0, std::min(static_cast<int>(fy), maxY));

    // Update pick UBO slot 0 with current camera before pick pass
    const glm::mat4 view = camera_.GetViewMatrix();
    const glm::mat4 proj = camera_.GetProjectionMatrix();
    r.descMgr().updateUniformBuffer(0, view, proj);

    const uint32_t id = r.pickSys().runPick(r.ctx(), r.rpMgr(), r.fbMgr(), r.pipeMgr(),
                                            r.descMgr().getBoxDescriptorSet(0),
                                            sceneMgr_, mainModelTransform.position,
                                            r.swapChain().getExtent(),
                                            static_cast<uint32_t>(px),
                                            static_cast<uint32_t>(py));

    if (id == SceneManager::kPickIdNone) return;
    if (id == SceneManager::kPickIdMainModel) {
        mainModelSelected  = true;
        selectedMaterialId = firstRenderedModelMaterialId();
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

void EngineRuntime::tryBeginCameraFocusOnPick()
{
    glm::vec3 focus(0.f);
    float distance = 4.f;
    if (mainModelSelected) {
        const glm::vec3 ext = sceneMgr_.getModelBoundsMax() - sceneMgr_.getModelBoundsMin();
        if (glm::length(ext) < 1e-5f) { focus = mainModelTransform.position; distance = 5.f; }
        else {
            focus    = 0.5f * (sceneMgr_.getModelBoundsMin() + sceneMgr_.getModelBoundsMax())
                       + mainModelTransform.position;
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

void EngineRuntime::select(int what, RenderEntityId boxId)
{
    mainModelSelected  = false;
    pickedBoxEntityId  = 0;
    selectedMaterialId = kInvalidMaterialId;
    if (what == 1) {
        mainModelSelected  = true;
        selectedMaterialId = firstRenderedModelMaterialId();
    } else if (what == 2) {
        if (sceneMgr_.getBoxes().count(boxId) == 0) return;
        pickedBoxEntityId  = boxId;
        selectedMaterialId = sceneMgr_.getBoxMaterialId(boxId);
    }
}
