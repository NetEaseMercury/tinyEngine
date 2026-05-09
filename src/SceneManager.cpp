#include "SceneManager.hpp"
#include <tiny_obj_loader.h>
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <stdexcept>

void SceneManager::destroyBuf(const VulkanContext& ctx, VkBuffer& buf, VkDeviceMemory& mem)
{
    auto dev = ctx.getDevice();
    if (buf != VK_NULL_HANDLE) { vkDestroyBuffer(dev, buf, nullptr); buf = VK_NULL_HANDLE; }
    if (mem != VK_NULL_HANDLE) { vkFreeMemory(dev, mem, nullptr);    mem = VK_NULL_HANDLE; }
}

void SceneManager::loadModel(const std::string& path, const glm::vec3& position,
                              const BufferManager& bufMgr)
{
    modelPosition_ = position;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
        throw std::runtime_error("Failed to load model: " + path + "\n" + err + "\n" + warn);

    modelVertices_.clear();
    modelIndices_.clear();
    modelLocalBoundsMin_ = glm::vec3(FLT_MAX);
    modelLocalBoundsMax_ = glm::vec3(-FLT_MAX);

    std::unordered_map<Vertex, uint32_t, VertexHash> unique;
    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            Vertex v{};
            v.pos = { attrib.vertices[3 * idx.vertex_index + 0],
                      attrib.vertices[3 * idx.vertex_index + 1],
                      attrib.vertices[3 * idx.vertex_index + 2] };
            if (idx.texcoord_index >= 0) {
                v.texCoord = { attrib.texcoords[2 * idx.texcoord_index + 0],
                               1.0f - attrib.texcoords[2 * idx.texcoord_index + 1] };
            }
            v.color = { 1.0f, 1.0f, 1.0f };

            modelLocalBoundsMin_ = glm::min(modelLocalBoundsMin_, v.pos);
            modelLocalBoundsMax_ = glm::max(modelLocalBoundsMax_, v.pos);

            if (!unique.count(v)) {
                unique[v] = static_cast<uint32_t>(modelVertices_.size());
                modelVertices_.push_back(v);
            }
            modelIndices_.push_back(unique[v]);
        }
    }

    bufMgr.createVertexBuffer(modelVertices_, vertexBuffer_, vertexMemory_);
    bufMgr.createIndexBuffer(modelIndices_, indexBuffer_, indexMemory_);
    modelIndexCount_ = static_cast<uint32_t>(modelIndices_.size());
}

void SceneManager::createCubeTemplate(const BufferManager& bufMgr)
{
    cubeTemplateVertices_.clear();
    cubeTemplateIndices_.clear();

    const float s = 0.5f;
    const glm::vec3 c = { 1.0f, 1.0f, 1.0f };

    auto addFace = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3) {
        uint32_t base = static_cast<uint32_t>(cubeTemplateVertices_.size());
        cubeTemplateVertices_.push_back({ v0, c, {0.f, 0.f} });
        cubeTemplateVertices_.push_back({ v1, c, {1.f, 0.f} });
        cubeTemplateVertices_.push_back({ v2, c, {1.f, 1.f} });
        cubeTemplateVertices_.push_back({ v3, c, {0.f, 1.f} });
        cubeTemplateIndices_.insert(cubeTemplateIndices_.end(),
            { base+0, base+1, base+2, base+2, base+3, base+0 });
    };

    addFace({-s,-s,+s},{+s,-s,+s},{+s,+s,+s},{-s,+s,+s});
    addFace({+s,-s,-s},{-s,-s,-s},{-s,+s,-s},{+s,+s,-s});
    addFace({-s,-s,-s},{-s,-s,+s},{-s,+s,+s},{-s,+s,-s});
    addFace({+s,-s,+s},{+s,-s,-s},{+s,+s,-s},{+s,+s,+s});
    addFace({-s,+s,+s},{+s,+s,+s},{+s,+s,-s},{-s,+s,-s});
    addFace({-s,-s,-s},{+s,-s,-s},{+s,-s,+s},{-s,-s,+s});

    bufMgr.createVertexBuffer(cubeTemplateVertices_, cubeVertexBuffer_, cubeVertexMemory_);
    bufMgr.createIndexBuffer(cubeTemplateIndices_, cubeIndexBuffer_, cubeIndexMemory_);
    cubeIndexCount_ = static_cast<uint32_t>(cubeTemplateIndices_.size());
}

void SceneManager::destroy(const VulkanContext& ctx)
{
    destroyBuf(ctx, instanceBuffer_,   instanceMemory_);
    destroyBuf(ctx, cubeIndexBuffer_,  cubeIndexMemory_);
    destroyBuf(ctx, cubeVertexBuffer_, cubeVertexMemory_);
    destroyBuf(ctx, indexBuffer_,      indexMemory_);
    destroyBuf(ctx, vertexBuffer_,     vertexMemory_);
}

void SceneManager::destroyModelBuffers(const VulkanContext& ctx)
{
    destroyBuf(ctx, indexBuffer_,  indexMemory_);
    destroyBuf(ctx, vertexBuffer_, vertexMemory_);
    modelIndexCount_ = 0;
}
RenderEntityId SceneManager::addBox(const glm::vec3& position,
                                     const VulkanContext& ctx, const BufferManager& bufMgr)
{
    RenderEntityId id = nextId_++;
    boxes_[id] = position;
    rebuildInstanceBuffer(ctx, bufMgr);
    return id;
}

bool SceneManager::removeBox(RenderEntityId id, const VulkanContext& ctx, const BufferManager& bufMgr)
{
    auto it = boxes_.find(id);
    if (it == boxes_.end()) return false;
    boxes_.erase(it);
    rebuildInstanceBuffer(ctx, bufMgr);
    return true;
}

glm::vec3 SceneManager::getBoxPosition(RenderEntityId id) const
{
    auto it = boxes_.find(id);
    return (it != boxes_.end()) ? it->second : glm::vec3(0.0f);
}

void SceneManager::setBoxPosition(RenderEntityId id, const glm::vec3& pos,
                                   const VulkanContext& ctx, const BufferManager& bufMgr)
{
    if (!boxes_.count(id)) return;
    boxes_[id] = pos;
    rebuildInstanceBuffer(ctx, bufMgr);
}

void SceneManager::rebuildInstanceBuffer(const VulkanContext& ctx, const BufferManager& bufMgr)
{
    vkDeviceWaitIdle(ctx.getDevice());
    destroyBuf(ctx, instanceBuffer_, instanceMemory_);
    boxRangeEntityIds_.clear();

    std::vector<std::pair<RenderEntityId, glm::vec3>> sorted(boxes_.begin(), boxes_.end());
    std::sort(sorted.begin(), sorted.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });

    instanceCount_ = static_cast<uint32_t>(sorted.size());
    if (instanceCount_ == 0) return;

    std::vector<InstanceData> instances;
    instances.reserve(instanceCount_);
    for (const auto& kv : sorted) {
        instances.push_back(InstanceData{ kv.second });
        boxRangeEntityIds_.push_back(kv.first);
    }

    bufMgr.createInstanceBuffer(instances.data(),
                                sizeof(InstanceData) * instanceCount_,
                                instanceBuffer_, instanceMemory_);
}
