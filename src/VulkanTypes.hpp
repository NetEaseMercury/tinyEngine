#pragma once
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <cstdint>

#define WIDTH  1280
#define HEIGHT 720

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };
const std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

struct UniformBufferObject {
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
    alignas(16) glm::vec4 materialTint;
    alignas(16) glm::vec4 boxMaterialTint;
    alignas(16) glm::vec4 emissive;     // rgb = emissive color, w = intensity
    // PBR additions ─ all aligned to 16 bytes (std140-friendly)
    alignas(16) glm::vec4 cameraPos;    // xyz = world camera position, w unused
    alignas(16) glm::vec4 lightDir;     // xyz = world-space *to-light* direction (normalized)
    alignas(16) glm::vec4 lightColor;   // rgb = radiance, a = ambient strength
    alignas(16) glm::vec4 pbrFactors;   // x=metallic, y=roughness, z=ao, w=normalScale
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    [[nodiscard]] bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

using RenderEntityId = uint64_t;
