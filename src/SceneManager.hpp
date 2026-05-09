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
};
