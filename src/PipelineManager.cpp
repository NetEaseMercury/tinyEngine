#include "PipelineManager.hpp"
#include "vectex.hpp"
#include <array>
#include <fstream>
#include <stdexcept>

std::vector<char> PipelineManager::readFile(const std::string& path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) return {};
    auto size = file.tellg();
    std::vector<char> buf(static_cast<size_t>(size));
    file.seekg(0);
    file.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

VkShaderModule PipelineManager::createShaderModule(const VulkanContext& ctx,
                                                   const std::vector<char>& code) const
{
    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod{};
    if (vkCreateShaderModule(ctx.getDevice(), &ci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module!");
    return mod;
}

void PipelineManager::create(const VulkanContext& ctx, const RenderPassManager& rpMgr,
                             const std::string& vertSpv, const std::string& fragSpv,
                             const std::string& boxVertSpv, const std::string& boxFragSpv,
                             VkExtent2D extent)
{
    createDescriptorSetLayouts(ctx);
    createMainPipeline(ctx, rpMgr.getMainRenderPass(), vertSpv, fragSpv, extent);
    createBoxPipeline(ctx, rpMgr.getMainRenderPass(), boxVertSpv, boxFragSpv, extent);

    // derive pick shader path from vert path dir
    const auto p = vertSpv.find_last_of("/\\");
    const std::string dir = (p != std::string::npos) ? vertSpv.substr(0, p + 1) : "";
    createPickPipeline(ctx, rpMgr.getPickRenderPass(), dir + "pick_vert.spv", extent);
}

void PipelineManager::recreate(const VulkanContext& ctx, const RenderPassManager& rpMgr,
                               const std::string& vertSpv, const std::string& fragSpv,
                               const std::string& boxVertSpv, const std::string& boxFragSpv,
                               VkExtent2D extent)
{
    destroyPipelines(ctx);
    createMainPipeline(ctx, rpMgr.getMainRenderPass(), vertSpv, fragSpv, extent);
    createBoxPipeline(ctx, rpMgr.getMainRenderPass(), boxVertSpv, boxFragSpv, extent);
    const auto p = vertSpv.find_last_of("/\\");
    const std::string dir = (p != std::string::npos) ? vertSpv.substr(0, p + 1) : "";
    createPickPipeline(ctx, rpMgr.getPickRenderPass(), dir + "pick_vert.spv", extent);
}

void PipelineManager::destroy(const VulkanContext& ctx)
{
    destroyPipelines(ctx);
    auto dev = ctx.getDevice();
    if (boxDescSetLayout_  != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, boxDescSetLayout_,  nullptr); boxDescSetLayout_  = VK_NULL_HANDLE; }
    if (mainDescSetLayout_ != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(dev, mainDescSetLayout_, nullptr); mainDescSetLayout_ = VK_NULL_HANDLE; }
}

void PipelineManager::destroyPipelines(const VulkanContext& ctx)
{
    auto dev = ctx.getDevice();
    if (pickPipeline_       != VK_NULL_HANDLE) { vkDestroyPipeline(dev, pickPipeline_, nullptr);            pickPipeline_       = VK_NULL_HANDLE; }
    if (pickPipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, pickPipelineLayout_, nullptr); pickPipelineLayout_ = VK_NULL_HANDLE; }
    if (boxPipeline_        != VK_NULL_HANDLE) { vkDestroyPipeline(dev, boxPipeline_, nullptr);             boxPipeline_        = VK_NULL_HANDLE; }
    if (boxPipelineLayout_  != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, boxPipelineLayout_, nullptr);  boxPipelineLayout_  = VK_NULL_HANDLE; }
    if (mainPipeline_       != VK_NULL_HANDLE) { vkDestroyPipeline(dev, mainPipeline_, nullptr);            mainPipeline_       = VK_NULL_HANDLE; }
    if (mainPipelineLayout_ != VK_NULL_HANDLE) { vkDestroyPipelineLayout(dev, mainPipelineLayout_, nullptr); mainPipelineLayout_ = VK_NULL_HANDLE; }
}

void PipelineManager::createDescriptorSetLayouts(const VulkanContext& ctx)
{
    // Main: UBO + albedo sampler + normal sampler
    {
        VkDescriptorSetLayoutBinding ubo{};
        ubo.binding         = 0; ubo.descriptorCount = 1;
        ubo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding tex{};
        tex.binding         = 1; tex.descriptorCount = 1;
        tex.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        tex.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutBinding nrm{};
        nrm.binding         = 2; nrm.descriptorCount = 1;
        nrm.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        nrm.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 3> bindings{ ubo, tex, nrm };
        VkDescriptorSetLayoutCreateInfo li{};
        li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = static_cast<uint32_t>(bindings.size());
        li.pBindings    = bindings.data();
        if (vkCreateDescriptorSetLayout(ctx.getDevice(), &li, nullptr, &mainDescSetLayout_) != VK_SUCCESS)
            throw std::runtime_error("Failed to create main descriptor set layout!");
    }
    // Box: UBO only
    {
        VkDescriptorSetLayoutBinding ubo{};
        ubo.binding         = 0; ubo.descriptorCount = 1;
        ubo.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        ubo.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo li{};
        li.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        li.bindingCount = 1; li.pBindings = &ubo;
        if (vkCreateDescriptorSetLayout(ctx.getDevice(), &li, nullptr, &boxDescSetLayout_) != VK_SUCCESS)
            throw std::runtime_error("Failed to create box descriptor set layout!");
    }
}

static VkPipelineShaderStageCreateInfo makeStage(VkShaderStageFlagBits stage, VkShaderModule mod)
{
    VkPipelineShaderStageCreateInfo s{};
    s.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    s.stage  = stage;
    s.module = mod;
    s.pName  = "main";
    return s;
}

void PipelineManager::createMainPipeline(const VulkanContext& ctx, VkRenderPass renderPass,
                                         const std::string& vertSpv, const std::string& fragSpv,
                                         VkExtent2D extent)
{
    auto vc = readFile(vertSpv);
    auto fc = readFile(fragSpv);
    if (vc.empty() || fc.empty()) throw std::runtime_error("Failed to read main shader SPIR-V!");
    VkShaderModule vm = createShaderModule(ctx, vc);
    VkShaderModule fm = createShaderModule(ctx, fc);
    VkPipelineShaderStageCreateInfo stages[] = { makeStage(VK_SHADER_STAGE_VERTEX_BIT, vm),
                                                  makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, fm) };

    auto bindDesc = Vertex::getBindingDescription();
    auto attrDesc = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount   = 1;
    vin.pVertexBindingDescriptions      = &bindDesc;
    vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size());
    vin.pVertexAttributeDescriptions    = attrDesc.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0.f, 0.f, (float)extent.width, (float)extent.height, 0.f, 1.f };
    VkRect2D sc{ {0,0}, extent };
    VkPipelineViewportStateCreateInfo vpState{};
    vpState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.viewportCount = 1; vpState.pViewports = &vp;
    vpState.scissorCount  = 1; vpState.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.f;
    rs.cullMode  = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp  = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pcr.size       = sizeof(glm::mat4);

    VkPipelineLayoutCreateInfo pli{};
    pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount         = 1; pli.pSetLayouts = &mainDescSetLayout_;
    pli.pushConstantRangeCount = 1; pli.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(ctx.getDevice(), &pli, nullptr, &mainPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create main pipeline layout!");

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount          = 2; gp.pStages = stages;
    gp.pVertexInputState   = &vin; gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vpState; gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms; gp.pDepthStencilState = &ds;
    gp.pColorBlendState    = &cb;
    gp.layout              = mainPipelineLayout_;
    gp.renderPass          = renderPass;
    if (vkCreateGraphicsPipelines(ctx.getDevice(), VK_NULL_HANDLE, 1, &gp, nullptr, &mainPipeline_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create main graphics pipeline!");

    vkDestroyShaderModule(ctx.getDevice(), vm, nullptr);
    vkDestroyShaderModule(ctx.getDevice(), fm, nullptr);
}

void PipelineManager::createBoxPipeline(const VulkanContext& ctx, VkRenderPass renderPass,
                                         const std::string& vertSpv, const std::string& fragSpv,
                                         VkExtent2D extent)
{
    auto vc = readFile(vertSpv);
    auto fc = readFile(fragSpv);
    if (vc.empty() || fc.empty()) throw std::runtime_error("Failed to read box shader SPIR-V!");
    VkShaderModule vm = createShaderModule(ctx, vc);
    VkShaderModule fm = createShaderModule(ctx, fc);
    VkPipelineShaderStageCreateInfo stages[] = { makeStage(VK_SHADER_STAGE_VERTEX_BIT, vm),
                                                  makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, fm) };

    auto vertBind = Vertex::getBindingDescription();
    auto instBind = InstanceData::getBindingDescription();
    std::array<VkVertexInputBindingDescription, 2> binds{ vertBind, instBind };
    auto vertAttr = Vertex::getAttributeDescriptions();
    auto instAttr = InstanceData::getAttributeDescription();
    std::array<VkVertexInputAttributeDescription, 4> attrs{
        vertAttr[0], vertAttr[1], vertAttr[2], instAttr };

    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount   = static_cast<uint32_t>(binds.size());
    vin.pVertexBindingDescriptions      = binds.data();
    vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vin.pVertexAttributeDescriptions    = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0.f, 0.f, (float)extent.width, (float)extent.height, 0.f, 1.f };
    VkRect2D sc{ {0,0}, extent };
    VkPipelineViewportStateCreateInfo vpState{};
    vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.viewportCount = 1; vpState.pViewports = &vp;
    vpState.scissorCount  = 1; vpState.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.f;
    rs.cullMode  = VK_CULL_MODE_BACK_BIT;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp  = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cba.blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkPipelineLayoutCreateInfo pli{};
    pli.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount = 1; pli.pSetLayouts = &boxDescSetLayout_;
    if (vkCreatePipelineLayout(ctx.getDevice(), &pli, nullptr, &boxPipelineLayout_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create box pipeline layout!");

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount          = 2; gp.pStages = stages;
    gp.pVertexInputState   = &vin; gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vpState; gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms; gp.pDepthStencilState = &ds;
    gp.pColorBlendState    = &cb;
    gp.layout              = boxPipelineLayout_;
    gp.renderPass          = renderPass;
    if (vkCreateGraphicsPipelines(ctx.getDevice(), VK_NULL_HANDLE, 1, &gp, nullptr, &boxPipeline_) != VK_SUCCESS)
        throw std::runtime_error("Failed to create box graphics pipeline!");

    vkDestroyShaderModule(ctx.getDevice(), vm, nullptr);
    vkDestroyShaderModule(ctx.getDevice(), fm, nullptr);
}

void PipelineManager::createPickPipeline(const VulkanContext& ctx, VkRenderPass pickRenderPass,
                                          const std::string& pickVertSpv, VkExtent2D extent)
{
    const std::string dir = [&]() {
        auto p = pickVertSpv.find_last_of("/\\");
        return (p != std::string::npos) ? pickVertSpv.substr(0, p + 1) : std::string{};
    }();
    const std::string pickFragSpv = dir + "pick_frag.spv";

    auto vc = readFile(pickVertSpv);
    auto fc = readFile(pickFragSpv);
    if (vc.empty() || fc.empty()) throw std::runtime_error("Failed to read pick shader SPIR-V!");
    VkShaderModule vm = createShaderModule(ctx, vc);
    VkShaderModule fm = createShaderModule(ctx, fc);
    VkPipelineShaderStageCreateInfo stages[] = { makeStage(VK_SHADER_STAGE_VERTEX_BIT, vm),
                                                  makeStage(VK_SHADER_STAGE_FRAGMENT_BIT, fm) };

    auto bindDesc = Vertex::getBindingDescription();
    auto attrDesc = Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount   = 1; vin.pVertexBindingDescriptions = &bindDesc;
    vin.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size());
    vin.pVertexAttributeDescriptions    = attrDesc.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0.f, 0.f, (float)extent.width, (float)extent.height, 0.f, 1.f };
    VkRect2D sc{ {0,0}, extent };
    VkPipelineViewportStateCreateInfo vpState{};
    vpState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vpState.viewportCount = 1; vpState.pViewports = &vp;
    vpState.scissorCount  = 1; vpState.pScissors  = &sc;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.lineWidth = 1.f;
    rs.cullMode  = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp  = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState cba{};
    cba.blendEnable    = VK_FALSE;
    cba.colorWriteMask = VK_COLOR_COMPONENT_R_BIT;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &cba;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.size       = 68u; // sizeof(mat4) + sizeof(uint32_t) padded to 68

    VkPipelineLayoutCreateInfo pli{};
    pli.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pli.setLayoutCount         = 1; pli.pSetLayouts = &boxDescSetLayout_;
    pli.pushConstantRangeCount = 1; pli.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(ctx.getDevice(), &pli, nullptr, &pickPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx.getDevice(), vm, nullptr);
        vkDestroyShaderModule(ctx.getDevice(), fm, nullptr);
        throw std::runtime_error("Failed to create pick pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo gp{};
    gp.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gp.stageCount          = 2; gp.pStages = stages;
    gp.pVertexInputState   = &vin; gp.pInputAssemblyState = &ia;
    gp.pViewportState      = &vpState; gp.pRasterizationState = &rs;
    gp.pMultisampleState   = &ms; gp.pDepthStencilState = &ds;
    gp.pColorBlendState    = &cb;
    gp.layout              = pickPipelineLayout_;
    gp.renderPass          = pickRenderPass;
    if (vkCreateGraphicsPipelines(ctx.getDevice(), VK_NULL_HANDLE, 1, &gp, nullptr, &pickPipeline_) != VK_SUCCESS) {
        vkDestroyShaderModule(ctx.getDevice(), vm, nullptr);
        vkDestroyShaderModule(ctx.getDevice(), fm, nullptr);
        throw std::runtime_error("Failed to create pick graphics pipeline!");
    }

    vkDestroyShaderModule(ctx.getDevice(), vm, nullptr);
    vkDestroyShaderModule(ctx.getDevice(), fm, nullptr);
}
