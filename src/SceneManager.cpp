#include "SceneManager.hpp"
#include <tiny_obj_loader.h>
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <stdexcept>

namespace {
bool endsWithIgnoreCase(const std::string& s, const char* suffix) {
    const size_t n = std::strlen(suffix);
    if (s.size() < n) return false;
    for (size_t i = 0; i < n; ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(s[s.size() - n + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) return false;
    }
    return true;
}
} // namespace

void SceneManager::destroyBuf(const VulkanContext& ctx, VkBuffer& buf, VkDeviceMemory& mem)
{
    auto dev = ctx.getDevice();
    if (buf != VK_NULL_HANDLE) { vkDestroyBuffer(dev, buf, nullptr); buf = VK_NULL_HANDLE; }
    if (mem != VK_NULL_HANDLE) { vkFreeMemory(dev, mem, nullptr);    mem = VK_NULL_HANDLE; }
}

void SceneManager::loadModel(const std::string& path, const glm::vec3& position,
                              const BufferManager& bufMgr)
{
    // Dispatch by file extension. glTF (.gltf JSON / .glb binary) -> cgltf,
    // anything else falls back to the original tinyobjloader path.
    if (endsWithIgnoreCase(path, ".gltf") || endsWithIgnoreCase(path, ".glb")) {
        loadModelFromGltf(path, position, bufMgr);
    } else {
        loadModelFromObj(path, position, bufMgr);
    }
}

void SceneManager::loadModelFromObj(const std::string& path, const glm::vec3& position,
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

void SceneManager::loadModelFromGltf(const std::string& path, const glm::vec3& position,
                                      const BufferManager& bufMgr)
{
    modelPosition_ = position;

    cgltf_options opts{};
    cgltf_data*   data = nullptr;
    cgltf_result  r = cgltf_parse_file(&opts, path.c_str(), &data);
    if (r != cgltf_result_success || !data) {
        throw std::runtime_error("Failed to parse glTF: " + path);
    }
    // Pulls in external .bin / embedded base64 / .glb chunks.
    if (cgltf_load_buffers(&opts, data, path.c_str()) != cgltf_result_success) {
        cgltf_free(data);
        throw std::runtime_error("Failed to load glTF buffers: " + path);
    }

    modelVertices_.clear();
    modelIndices_.clear();
    modelLocalBoundsMin_ = glm::vec3(FLT_MAX);
    modelLocalBoundsMax_ = glm::vec3(-FLT_MAX);

    std::unordered_map<Vertex, uint32_t, VertexHash> unique;

    // Walk every primitive of every mesh and append into one vertex/index pool.
    // Multi-material primitives are merged (current engine has one descriptor set
    // per main model); follow-up work could split per-primitive.
    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
        const cgltf_mesh& mesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi) {
            const cgltf_primitive& prim = mesh.primitives[pi];
            if (prim.type != cgltf_primitive_type_triangles || !prim.indices) continue;

            const cgltf_accessor* posAcc = nullptr;
            const cgltf_accessor* uvAcc  = nullptr;
            for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
                const cgltf_attribute& at = prim.attributes[a];
                if (at.type == cgltf_attribute_type_position)  posAcc = at.data;
                else if (at.type == cgltf_attribute_type_texcoord && !uvAcc) uvAcc = at.data;
            }
            if (!posAcc) continue;

            const cgltf_size vcount = posAcc->count;
            std::vector<Vertex> primVerts(vcount);
            for (cgltf_size i = 0; i < vcount; ++i) {
                float p[3]{};
                cgltf_accessor_read_float(posAcc, i, p, 3);
                Vertex v{};
                v.pos      = { p[0], p[1], p[2] };
                v.color    = { 1.f, 1.f, 1.f };
                v.texCoord = { 0.f, 0.f };
                if (uvAcc) {
                    float uv[2]{};
                    cgltf_accessor_read_float(uvAcc, i, uv, 2);
                    // glTF already uses top-left UV origin (Vulkan-compatible),
                    // no V-flip needed unlike the .obj path.
                    v.texCoord = { uv[0], uv[1] };
                }
                primVerts[i] = v;
                modelLocalBoundsMin_ = glm::min(modelLocalBoundsMin_, v.pos);
                modelLocalBoundsMax_ = glm::max(modelLocalBoundsMax_, v.pos);
            }

            const cgltf_accessor* idxAcc = prim.indices;
            for (cgltf_size i = 0; i < idxAcc->count; ++i) {
                const cgltf_size srcIdx = cgltf_accessor_read_index(idxAcc, i);
                if (srcIdx >= primVerts.size()) continue;
                const Vertex& v = primVerts[srcIdx];
                auto it = unique.find(v);
                uint32_t dst;
                if (it == unique.end()) {
                    dst = static_cast<uint32_t>(modelVertices_.size());
                    unique.emplace(v, dst);
                    modelVertices_.push_back(v);
                } else {
                    dst = it->second;
                }
                modelIndices_.push_back(dst);
            }
        }
    }

    cgltf_free(data);

    if (modelVertices_.empty() || modelIndices_.empty()) {
        throw std::runtime_error("glTF has no usable triangle data: " + path);
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
    boxMaterialIds_.erase(id);
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

uint32_t SceneManager::getBoxMaterialId(RenderEntityId eid) const
{
    auto it = boxMaterialIds_.find(eid);
    return (it != boxMaterialIds_.end()) ? it->second : 0u;
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
