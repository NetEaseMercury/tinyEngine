#pragma once
#ifndef BaseInclude
#define BaseInclude
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif // !GLM_ENABLE_EXPERIMENTAL
#include "vulkan/vulkan_core.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include "vk_mem_alloc.h"

#include <imgui.h>
#include <backends/imgui_impl_vulkan.h>
#include <backends/imgui_impl_glfw.h>

#include <optional>
#include <vector>
#include <iostream>
#include "vectex.hpp"
#include <unordered_map>
#include <set>
#include <array>
#endif // !BaseInclude
