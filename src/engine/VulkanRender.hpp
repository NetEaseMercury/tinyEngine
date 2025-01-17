#pragma once
#include "../base/BaseInclude.hpp"

class VulkanRenderer {
public:
	VulkanRenderer() = default;
	~VulkanRenderer() = default;
public:
	void init();
	void cleanup();
	void drawFrame();

	void loadSceneToDevice();

	void cleanupSwapChainDependentObjects();
	void recreateSwapChainDenpentObjects();

private:
	void updateDynamicData();
	void drawObjectsCommands(VkCommandBuffer cmd, VkFramebuffer framebuffer);
	void createMainRenderPass();
	void fillConstantGlobalBuffers(const Scene* scene);
	void createComputePipeline(const char* shaderPath, VkPipeline& pipeline, VkPipelineLayout& layout, ShaderPass& shaderPass);
	void createCullingDescriptors(uint32_t nbObjects);
	void createDepthPyramidDescriptors();
	void createFramebuffers();
	void createDepthPyramid();
	void createDepthSampler();
	void createBarriers();
	void computeDepthPyramid(VkCommandBuffer commandBuffer);
	void createGlobalDescriptors(uint32_t nbObjects);
};