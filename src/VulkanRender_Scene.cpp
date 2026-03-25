/**
 * @file VulkanRender_Scene.cpp
 * @brief Scene: matrices for UI, picking, OBJ load, cube template, merged geometry, boxes.
 */
#include "VulkanRender.hpp"
#include "VulkanRender_Globals.hpp"
#include <tiny_obj_loader.h>
#include <GLFW/glfw3.h>
#include <cfloat>
#include <cmath>
#include <utility>
#include <algorithm>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

/** @brief Current camera view (see VulkanRender.hpp) */
glm::mat4 VulkanRender::getSceneViewMatrix()
{
	return camera.GetViewMatrix();
}

/** @brief Vulkan perspective with Y flip for rendering */
glm::mat4 VulkanRender::getSceneProjMatrix()
{
	glm::mat4 proj = glm::perspective(glm::radians(45.0f),
		swapChainExtent.width / static_cast<float>(swapChainExtent.height),
		0.1f, 500.0f);
	proj[1][1] *= -1;
	return proj;
}

/** @brief OpenGL-style perspective for ImGuizmo only */
glm::mat4 VulkanRender::getSceneProjMatrixForImGuizmo()
{
	return glm::perspective(glm::radians(45.0f),
		swapChainExtent.width / static_cast<float>(swapChainExtent.height),
		0.1f, 500.0f);
}

void VulkanRender::tryPickMainModel(float cursorX, float cursorY)
{
	mainModelSelected = false;
	pickedBoxEntityId = 0;

	if (pickFramebuffer == VK_NULL_HANDLE) {
		return;
	}

	int winW = 0;
	int winH = 0;
	int fbW = 0;
	int fbH = 0;
	glfwGetWindowSize(window, &winW, &winH);
	glfwGetFramebufferSize(window, &fbW, &fbH);
	if (winW <= 0 || winH <= 0 || fbW <= 0 || fbH <= 0) {
		return;
	}

	const float fx = cursorX * static_cast<float>(fbW) / static_cast<float>(winW);
	const float fy = cursorY * static_cast<float>(fbH) / static_cast<float>(winH);
	int px = static_cast<int>(std::floor(fx));
	int py = static_cast<int>(std::floor(fy));
	const int maxX = static_cast<int>(swapChainExtent.width) - 1;
	const int maxY = static_cast<int>(swapChainExtent.height) - 1;
	if (maxX < 0 || maxY < 0) {
		return;
	}
	px = std::max(0, std::min(px, maxX));
	py = std::max(0, std::min(py, maxY));

	uint32_t id = kPickIdNone;
	runPickPassReadId(static_cast<uint32_t>(px), static_cast<uint32_t>(py), id);

	if (id == kPickIdNone) {
		return;
	}
	if (id == kPickIdMainModel) {
		mainModelSelected = true;
		return;
	}
	if (id >= kPickIdBoxBase) {
		const uint32_t idx = id - kPickIdBoxBase;
		if (idx < boxRangeEntityIds.size()) {
			pickedBoxEntityId = boxRangeEntityIds[idx];
			mainModelSelected = false;
		}
	}
}

/** @brief Load OBJ into modelVertices/indices and recompute AABB; rebuild GPU buffers via caller chain */
void VulkanRender::loadModel(std::string modelPath, glm::vec3 position)
{
	(void)position;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string warn, err;

	if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, modelPath.c_str())) {
		std::string msg = "Failed to load model: " + modelPath;
		if (!err.empty()) {
			msg += "\n";
			msg += err;
		}
		if (!warn.empty()) {
			msg += "\n";
			msg += warn;
		}
		throw std::runtime_error(msg);
	}

	modelVertices.clear();
	modelIndices.clear();

	std::unordered_map<Vertex, uint32_t, VertexHash> uniqueVertices{};

	modelLocalBoundsMin = glm::vec3(FLT_MAX);
	modelLocalBoundsMax = glm::vec3(-FLT_MAX);

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			Vertex vertex{};

			vertex.pos = {
				attrib.vertices[3 * index.vertex_index + 0],
				attrib.vertices[3 * index.vertex_index + 1],
				attrib.vertices[3 * index.vertex_index + 2]
			};

			if (index.texcoord_index >= 0) {
				vertex.texCoord = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					1.0f - attrib.texcoords[2 * index.texcoord_index + 1],
				};
			}
			else {
				vertex.texCoord = { 0.0f, 0.0f };
			}

			vertex.color = { 1.0f, 1.0f, 1.0f };

			modelLocalBoundsMin = glm::min(modelLocalBoundsMin, vertex.pos);
			modelLocalBoundsMax = glm::max(modelLocalBoundsMax, vertex.pos);

			if (uniqueVertices.count(vertex) == 0) {
				uniqueVertices[vertex] = static_cast<uint32_t>(modelVertices.size());
				modelVertices.push_back(vertex);
			}

			modelIndices.push_back(uniqueVertices[vertex]);
		}
	}

	rebuildCombinedGeometryCPU();
}

/** @brief Fill cubeTemplateVertices/Indices with unit cube (half-extent 0.5) for instancing */
void VulkanRender::createCubeTemplate()
{
	cubeTemplateVertices.clear();
	cubeTemplateIndices.clear();

	const float s = 0.5f;
	const glm::vec3 c = { 1.0f, 1.0f, 1.0f };

	auto addFace = [&](const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2, const glm::vec3& v3) {
		uint32_t base = static_cast<uint32_t>(cubeTemplateVertices.size());

		cubeTemplateVertices.push_back(Vertex{ v0, c, glm::vec2(0.0f, 0.0f) });
		cubeTemplateVertices.push_back(Vertex{ v1, c, glm::vec2(1.0f, 0.0f) });
		cubeTemplateVertices.push_back(Vertex{ v2, c, glm::vec2(1.0f, 1.0f) });
		cubeTemplateVertices.push_back(Vertex{ v3, c, glm::vec2(0.0f, 1.0f) });

		cubeTemplateIndices.push_back(base + 0);
		cubeTemplateIndices.push_back(base + 1);
		cubeTemplateIndices.push_back(base + 2);
		cubeTemplateIndices.push_back(base + 2);
		cubeTemplateIndices.push_back(base + 3);
		cubeTemplateIndices.push_back(base + 0);
	};

	addFace(glm::vec3(-s, -s, +s), glm::vec3(+s, -s, +s), glm::vec3(+s, +s, +s), glm::vec3(-s, +s, +s));
	addFace(glm::vec3(+s, -s, -s), glm::vec3(-s, -s, -s), glm::vec3(-s, +s, -s), glm::vec3(+s, +s, -s));
	addFace(glm::vec3(-s, -s, -s), glm::vec3(-s, -s, +s), glm::vec3(-s, +s, +s), glm::vec3(-s, +s, -s));
	addFace(glm::vec3(+s, -s, +s), glm::vec3(+s, -s, -s), glm::vec3(+s, +s, -s), glm::vec3(+s, +s, +s));
	addFace(glm::vec3(-s, +s, +s), glm::vec3(+s, +s, +s), glm::vec3(+s, +s, -s), glm::vec3(-s, +s, -s));
	addFace(glm::vec3(-s, -s, -s), glm::vec3(+s, -s, -s), glm::vec3(+s, -s, +s), glm::vec3(-s, -s, +s));
}

/** @brief Merge main mesh + sorted boxes into single vertices/indices; fill boxIndexRanges */
void VulkanRender::rebuildCombinedGeometryCPU()
{
	vertices.clear();
	indices.clear();
	boxIndexRanges.clear();
	boxRangeEntityIds.clear();

	vertices = modelVertices;
	indices = modelIndices;
	modelIndexCount = static_cast<uint32_t>(modelIndices.size());

	std::vector<std::pair<RenderEntityId, glm::vec3>> boxList(boxes.begin(), boxes.end());
	std::sort(boxList.begin(), boxList.end(),
		[](const std::pair<RenderEntityId, glm::vec3>& a, const std::pair<RenderEntityId, glm::vec3>& b) {
			return a.first < b.first;
		});

	for (const auto& kv : boxList) {
		const glm::vec3& pos = kv.second;
		const uint32_t baseVertex = static_cast<uint32_t>(vertices.size());
		for (const Vertex& tmpl : cubeTemplateVertices) {
			Vertex v = tmpl;
			v.pos += pos;
			vertices.push_back(v);
		}
		const uint32_t firstIndex = static_cast<uint32_t>(indices.size());
		for (uint32_t idx : cubeTemplateIndices) {
			indices.push_back(baseVertex + idx);
		}
		boxIndexRanges.push_back({ firstIndex, static_cast<uint32_t>(cubeTemplateIndices.size()) });
		boxRangeEntityIds.push_back(kv.first);
	}
}

/** @brief Free GPU vertex/index buffers for merged geometry */
void VulkanRender::destroyGeometryBuffers()
{
	if (vertexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vertexBuffer = VK_NULL_HANDLE;
	}
	if (vertexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, vertexBufferMemory, nullptr);
		vertexBufferMemory = VK_NULL_HANDLE;
	}
	if (indexBuffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, indexBuffer, nullptr);
		indexBuffer = VK_NULL_HANDLE;
	}
	if (indexBufferMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, indexBufferMemory, nullptr);
		indexBufferMemory = VK_NULL_HANDLE;
	}
}

/** @brief Idle, rebuild CPU mesh, recreate VBO/IBO and re-record command buffers */
void VulkanRender::recreateGeometryBuffersAndCommandBuffers()
{
	vkDeviceWaitIdle(device);
	rebuildCombinedGeometryCPU();
	destroyGeometryBuffers();
	createVertexBuffer();
	createIndexBuffer();
	if (!commandBuffers.empty()) {
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
		commandBuffers.clear();
	}
	createCommandBuffers();
}

/** @brief Register box position, bump id, rebuild geometry (see VulkanRender.hpp) */
VulkanRender::RenderEntityId VulkanRender::addBox(const glm::vec3& position)
{
	RenderEntityId id = nextRenderEntityId++;
	boxes[id] = position;
	recreateGeometryBuffersAndCommandBuffers();
	return id;
}

/** @brief Erase box by id; clear pick if needed; rebuild geometry */
bool VulkanRender::removeBox(RenderEntityId id)
{
	auto it = boxes.find(id);
	if (it == boxes.end()) {
		return false;
	}
	boxes.erase(it);
	if (pickedBoxEntityId == id) {
		pickedBoxEntityId = 0;
	}
	recreateGeometryBuffersAndCommandBuffers();
	return true;
}

/** @brief Return box center or zero if id missing */
glm::vec3 VulkanRender::getBoxPosition(RenderEntityId id) const
{
	const auto it = boxes.find(id);
	if (it == boxes.end()) {
		return glm::vec3(0.0f);
	}
	return it->second;
}

/** @brief Update box transform and rebuild merged buffers */
void VulkanRender::setBoxPosition(RenderEntityId id, const glm::vec3& worldPosition)
{
	if (boxes.find(id) == boxes.end()) {
		return;
	}
	boxes[id] = worldPosition;
	recreateGeometryBuffersAndCommandBuffers();
}
