#pragma once
#include "VulkanContext.hpp"
#include "RenderPassManager.hpp"
#include <string>
#include <vector>

class PipelineManager {
public:
    void create(const VulkanContext& ctx, const RenderPassManager& rpMgr,
                const std::string& vertSpv, const std::string& fragSpv,
                const std::string& boxVertSpv, const std::string& boxFragSpv,
                VkExtent2D extent);
    void recreate(const VulkanContext& ctx, const RenderPassManager& rpMgr,
                  const std::string& vertSpv, const std::string& fragSpv,
                  const std::string& boxVertSpv, const std::string& boxFragSpv,
                  VkExtent2D extent);
    void destroy(const VulkanContext& ctx);

    VkDescriptorSetLayout getMainDescSetLayout() const { return mainDescSetLayout_; }
    VkDescriptorSetLayout getBoxDescSetLayout()  const { return boxDescSetLayout_; }
    VkPipelineLayout      getMainPipelineLayout() const { return mainPipelineLayout_; }
    VkPipelineLayout      getBoxPipelineLayout()  const { return boxPipelineLayout_; }
    VkPipelineLayout      getPickPipelineLayout() const { return pickPipelineLayout_; }
    VkPipeline            getMainPipeline()       const { return mainPipeline_; }
    VkPipeline            getBoxPipeline()        const { return boxPipeline_; }
    VkPipeline            getPickPipeline()       const { return pickPipeline_; }

    static std::vector<char> readFile(const std::string& path);
    VkShaderModule createShaderModule(const VulkanContext& ctx, const std::vector<char>& code) const;

    void destroyPipelines(const VulkanContext& ctx);

private:
    VkDescriptorSetLayout mainDescSetLayout_{};
    VkDescriptorSetLayout boxDescSetLayout_{};
    VkPipelineLayout      mainPipelineLayout_{};
    VkPipelineLayout      boxPipelineLayout_{};
    VkPipelineLayout      pickPipelineLayout_{};
    VkPipeline            mainPipeline_{};
    VkPipeline            boxPipeline_{};
    VkPipeline            pickPipeline_{};

    void createDescriptorSetLayouts(const VulkanContext& ctx);
    void createMainPipeline(const VulkanContext& ctx, VkRenderPass renderPass,
                            const std::string& vertSpv, const std::string& fragSpv,
                            VkExtent2D extent);
    void createBoxPipeline(const VulkanContext& ctx, VkRenderPass renderPass,
                           const std::string& vertSpv, const std::string& fragSpv,
                           VkExtent2D extent);
    void createPickPipeline(const VulkanContext& ctx, VkRenderPass pickRenderPass,
                            const std::string& vertSpv, VkExtent2D extent);
};
