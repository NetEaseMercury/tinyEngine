#pragma once
#include "VulkanContext.hpp"
#include "BufferManager.hpp"
#include "VulkanTypes.hpp"
#include "vectex.hpp"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

class SceneManager {
public:
    static constexpr uint32_t kPickIdNone      = 0;
    static constexpr uint32_t kPickIdMainModel = 1;
    static constexpr uint32_t kPickIdBoxBase   = 2;

    // Sub-range of the main model index buffer corresponding to one glTF
    // primitive (or, for .obj loads, the whole mesh). Multiple SubMeshes
    // share the same vertex/index buffer; rendering iterates this list and
    // switches descriptor set per entry.
    struct SubMesh {
        uint32_t indexOffset = 0;
        uint32_t indexCount  = 0;
        // Index into modelSubMeshMaterials_; -1 means "use modelMaterialId_".
        int      materialSlot = -1;
    };

    void loadModel(const std::string& path, const glm::vec3& position, const BufferManager& bufMgr);
    void createCubeTemplate(const BufferManager& bufMgr);
    void destroyModelBuffers(const VulkanContext& ctx);
    void destroy(const VulkanContext& ctx);

    RenderEntityId addBox(const glm::vec3& position, const VulkanContext& ctx, const BufferManager& bufMgr);
    bool           removeBox(RenderEntityId id, const VulkanContext& ctx, const BufferManager& bufMgr);
    glm::vec3      getBoxPosition(RenderEntityId id) const;
    void           setBoxPosition(RenderEntityId id, const glm::vec3& pos,
                                  const VulkanContext& ctx, const BufferManager& bufMgr);

    VkBuffer getVertexBuffer()     const { return vertexBuffer_; }
    VkBuffer getIndexBuffer()      const { return indexBuffer_; }
    uint32_t getModelIndexCount()  const { return modelIndexCount_; }
    VkBuffer getCubeVertexBuffer() const { return cubeVertexBuffer_; }
    VkBuffer getCubeIndexBuffer()  const { return cubeIndexBuffer_; }
    uint32_t getCubeIndexCount()   const { return cubeIndexCount_; }
    VkBuffer getInstanceBuffer()   const { return instanceBuffer_; }
    uint32_t getInstanceCount()    const { return instanceCount_; }

    glm::vec3 getModelPosition()  const { return modelPosition_; }
    void setModelPosition(const glm::vec3& p) { modelPosition_ = p; }
    glm::vec3 getModelBoundsMin() const { return modelLocalBoundsMin_; }
    glm::vec3 getModelBoundsMax() const { return modelLocalBoundsMax_; }

    const std::vector<RenderEntityId>&                    getBoxRangeEntityIds() const { return boxRangeEntityIds_; }
    const std::unordered_map<RenderEntityId, glm::vec3>&  getBoxes()             const { return boxes_; }

    // Material assignment (uint32_t == MaterialId, avoids circular header dependency)
    void     setModelMaterialId(uint32_t id)                          { modelMaterialId_ = id; }
    uint32_t getModelMaterialId()                              const   { return modelMaterialId_; }
    void     setBoxMaterialId(RenderEntityId eid, uint32_t id)        { boxMaterialIds_[eid] = id; }
    uint32_t getBoxMaterialId(RenderEntityId eid)              const;
    bool     hasBoxMaterialId(RenderEntityId eid)              const   { return boxMaterialIds_.count(eid) > 0; }

    // ── Sub-mesh API ─────────────────────────────────────────────────
    const std::vector<SubMesh>& getModelSubMeshes() const { return modelSubMeshes_; }
    // Per-slot material override; falls back to modelMaterialId_ when the
    // submesh has no slot or the slot is unbound.
    uint32_t getModelSubMeshMaterialId(int slot) const {
        if (slot < 0 || slot >= (int)modelSubMeshMaterials_.size()) return modelMaterialId_;
        const uint32_t m = modelSubMeshMaterials_[slot];
        return (m != 0u) ? m : modelMaterialId_;
    }
    void setModelSubMeshMaterialId(int slot, uint32_t id) {
        if (slot < 0) return;
        if ((int)modelSubMeshMaterials_.size() <= slot)
            modelSubMeshMaterials_.resize(slot + 1, 0u);
        modelSubMeshMaterials_[slot] = id;
    }
    // Auto-generated .ast paths from the last glTF load (relative to res/),
    // one per SubMesh slot. Empty entries -> the primitive had no material.
    const std::vector<std::string>& getModelAutoAstPaths() const { return modelAutoAstPaths_; }

private:
    std::vector<Vertex>    modelVertices_;
    std::vector<uint32_t>  modelIndices_;
    glm::vec3              modelLocalBoundsMin_{};
    glm::vec3              modelLocalBoundsMax_{};
    glm::vec3              modelPosition_{};

    std::vector<Vertex>    cubeTemplateVertices_;
    std::vector<uint32_t>  cubeTemplateIndices_;

    std::unordered_map<RenderEntityId, glm::vec3> boxes_;
    std::vector<RenderEntityId>                   boxRangeEntityIds_;
    RenderEntityId nextId_ = 1;

    uint32_t                                      modelMaterialId_ = 0;
    std::unordered_map<RenderEntityId, uint32_t>  boxMaterialIds_;

    // SubMesh ranges populated by loadModelFrom*; always non-empty after a
    // successful load (the .obj path emits exactly one entry covering the
    // entire mesh, the glTF path emits one per primitive).
    std::vector<SubMesh>     modelSubMeshes_;
    std::vector<uint32_t>    modelSubMeshMaterials_; // size = number of slots, 0 = unbound
    std::vector<std::string> modelAutoAstPaths_;     // "materials/<base>_<idx>.ast" (or empty)

    VkBuffer       vertexBuffer_{};     VkDeviceMemory vertexMemory_{};
    VkBuffer       indexBuffer_{};      VkDeviceMemory indexMemory_{};
    VkBuffer       cubeVertexBuffer_{}; VkDeviceMemory cubeVertexMemory_{};
    VkBuffer       cubeIndexBuffer_{};  VkDeviceMemory cubeIndexMemory_{};
    VkBuffer       instanceBuffer_{};   VkDeviceMemory instanceMemory_{};

    uint32_t modelIndexCount_ = 0;
    uint32_t cubeIndexCount_  = 0;
    uint32_t instanceCount_   = 0;

    void rebuildInstanceBuffer(const VulkanContext& ctx, const BufferManager& bufMgr);
    static void destroyBuf(const VulkanContext& ctx, VkBuffer& buf, VkDeviceMemory& mem);

    // Format-specific loaders, dispatched by file extension inside loadModel().
    void loadModelFromObj (const std::string& path, const glm::vec3& position, const BufferManager& bufMgr);
    void loadModelFromGltf(const std::string& path, const glm::vec3& position, const BufferManager& bufMgr);
};
