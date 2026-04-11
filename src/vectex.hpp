
#ifndef VERTEX
#define VERTEX
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <array>
#include <vulkan/vulkan_core.h>

// ���嶥����Ϣ
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    ~Vertex() = default;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct VertexHash {
    std::size_t operator()(const Vertex& vertex) const {
        auto hash1 = std::hash<glm::vec3>()(vertex.pos);
        auto hash2 = std::hash<glm::vec2>()(vertex.texCoord);
        auto hash3 = std::hash<glm::vec3>()(vertex.color);
        return hash1 ^ (hash2 << 1) ^ (hash3 << 2);
    }
};

/** @brief Per-instance data for GPU instanced box rendering (binding=1, instance rate) */
struct InstanceData {
    glm::vec3 position;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription desc{};
        desc.binding = 1;
        desc.stride = sizeof(InstanceData);
        desc.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return desc;
    }

    static VkVertexInputAttributeDescription getAttributeDescription() {
        VkVertexInputAttributeDescription attr{};
        attr.binding = 1;
        attr.location = 3;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = offsetof(InstanceData, position);
        return attr;
    }
};

#endif // !VERTEX
