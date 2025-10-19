#include "Buffer.h"
#include "DescriptorSet.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"

#include <iostream>
Pipeline::Pipeline(VulkanContext& InContext, const Specs& InSpecs) : _Specs(InSpecs), _Context(InContext)
{
    Init();
}
Pipeline::~Pipeline()
{
    Cleanup();
}
void Pipeline::Resize()
{
    Cleanup();
    Init();
}
void Pipeline::Cleanup()
{
    vkDestroyPipeline(_Context.GetDevice()->GetVKDevice(), _Pipeline, nullptr);
    vkDestroyPipelineLayout(_Context.GetDevice()->GetVKDevice(), _PipelineLayout, nullptr);
}

VkPipeline Pipeline::GetHandle() const
{
    return _Pipeline;
}
VkPipelineLayout Pipeline::GetPipelineLayout() const
{
    return _PipelineLayout;
}

void Pipeline::Init()
{
    _DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };

    // ------------------------------------------------------------------------
    // Shader stages
    // ------------------------------------------------------------------------
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    auto loadShader = [&](const std::string& path, VkShaderStageFlagBits stage) -> VkShaderModule
    {
        if (path.empty() || path == "None")
            return VK_NULL_HANDLE;

        auto           code   = Utils::ReadFile(path);
        VkShaderModule module = CreateShaderModule(code);

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage  = stage;
        stageInfo.module = module;
        stageInfo.pName  = "main";
        shaderStages.push_back(stageInfo);

        return module;
    };

    VkShaderModule vertShaderModule = loadShader(_Specs.VertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragShaderModule = loadShader(_Specs.FragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkShaderModule geomShaderModule = loadShader(_Specs.GeometryShaderPath, VK_SHADER_STAGE_GEOMETRY_BIT);

    // ------------------------------------------------------------------------
    // Vertex input
    // ------------------------------------------------------------------------
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = static_cast<uint32_t>(_Specs.VertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions      = _Specs.VertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(_Specs.VertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions    = _Specs.VertexAttributes.data();

    // ------------------------------------------------------------------------
    // Input assembly
    // ------------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = _Specs.PrimitiveTopology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ------------------------------------------------------------------------
    // Viewport & scissor
    // ------------------------------------------------------------------------
    VkViewport viewport{};
    if (_Specs.ViewportWidth == UINT32_MAX && _Specs.ViewportHeight == UINT32_MAX)
    {
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)_Context.GetSurface()->GetVKExtent().width;
        viewport.height   = (float)_Context.GetSurface()->GetVKExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    }
    else
    {
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = (float)_Specs.ViewportWidth;
        viewport.height   = (float)_Specs.ViewportHeight;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    }

    VkRect2D scissor{};
    scissor.offset       = { 0, 0 };
    scissor.extent.width = _Specs.ViewportWidth == UINT32_MAX ? _Context.GetSurface()->GetVKExtent().width : _Specs.ViewportWidth;
    scissor.extent.height =
        _Specs.ViewportHeight == UINT32_MAX ? _Context.GetSurface()->GetVKExtent().height : _Specs.ViewportHeight;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    // ------------------------------------------------------------------------
    // Rasterizer
    // ------------------------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = _Specs.PolygonMode;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = _Specs.CullMode;
    rasterizer.frontFace               = _Specs.FrontFace;
    rasterizer.depthBiasEnable         = _Specs.EnableDepthBias;
    rasterizer.depthBiasConstantFactor = _Specs.DepthBiasConstantFactor; // Optional
    rasterizer.depthBiasClamp          = _Specs.DepthBiasClamp; // Optional
    rasterizer.depthBiasSlopeFactor    = _Specs.DepthBiasSlopeFactor; // Optional

    // ------------------------------------------------------------------------
    // Multisampling
    // ------------------------------------------------------------------------
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable   = VK_FALSE;
    multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading      = 1.0f; // Optional
    multisampling.pSampleMask           = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable      = VK_FALSE; // Optional

    // ------------------------------------------------------------------------
    // Color blending
    // ------------------------------------------------------------------------
    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &_Specs.ColorBlendAttachmentState;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // ------------------------------------------------------------------------
    // Depth & stencil
    // ------------------------------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = _Specs.EnableDepthTesting;
    depthStencil.depthWriteEnable      = _Specs.EnableDepthWriting;
    depthStencil.depthCompareOp        = _Specs.DepthCompareOp;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds        = 0.0f; // Optional
    depthStencil.maxDepthBounds        = 1.0f; // Optional
    depthStencil.stencilTestEnable     = VK_FALSE;
    depthStencil.front                 = {}; // Optional
    depthStencil.back.compareOp        = VK_COMPARE_OP_ALWAYS; // Optional

    // ------------------------------------------------------------------------
    // Dynamic states
    // ------------------------------------------------------------------------
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(_DynamicStates.size());
    dynamicState.pDynamicStates    = _DynamicStates.data();

    // ------------------------------------------------------------------------
    // Pipeline layout
    // ------------------------------------------------------------------------
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_Specs.DescriptorSetLayout->GetDescriptorLayout();

    if (_Specs.PushConstantRanges.size() > 0)
    {
        // pushConstantRange.offset = _Specs.PushConstantOffset;
        // pushConstantRange.size = _Specs.PushConstantSize;
        // pushConstantRange.stageFlags = _Specs.PushConstantShaderStage;
        pipelineLayoutInfo.pushConstantRangeCount = _Specs.PushConstantRanges.size(); // Optional
        pipelineLayoutInfo.pPushConstantRanges    = _Specs.PushConstantRanges.data(); // Optional
    }
    else
    {
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges    = nullptr; // Optional
    }

    ASSERT(
        vkCreatePipelineLayout(_Context.GetDevice()->GetVKDevice(), &pipelineLayoutInfo, nullptr, &_PipelineLayout) == VK_SUCCESS,
        "Failed to create pipeline layout");

    // ------------------------------------------------------------------------
    // Graphics pipeline creation
    // ------------------------------------------------------------------------
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages             = shaderStages.data();
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = nullptr; // Optional
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.pDynamicState       = nullptr; // Optional
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = _PipelineLayout;
    pipelineInfo.renderPass          = _Specs.RenderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex   = -1; // Optional

    ASSERT(
        vkCreateGraphicsPipelines(_Context.GetDevice()->GetVKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_Pipeline) ==
            VK_SUCCESS,
        "Failed to create graphics pipeline!");

    // ------------------------------------------------------------------------
    // Cleanup temporary shader modules
    // ------------------------------------------------------------------------
    if (_Specs.FragmentShaderPath != "None")
    {
        vkDestroyShaderModule(_Context.GetDevice()->GetVKDevice(), fragShaderModule, nullptr);
    }
    if (_Specs.VertexShaderPath != "None")
    {
        vkDestroyShaderModule(_Context.GetDevice()->GetVKDevice(), vertShaderModule, nullptr);
    }
    if (_Specs.GeometryShaderPath != "None")
    {
        vkDestroyShaderModule(_Context.GetDevice()->GetVKDevice(), geomShaderModule, nullptr);
    }
}
VkShaderModule Pipeline::CreateShaderModule(const std::vector<char>& InShaderCode)
{
    VkShaderModuleCreateInfo createInfo{};

    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = InShaderCode.size();
    createInfo.pCode    = reinterpret_cast<const uint32_t*>(InShaderCode.data());

    VkShaderModule shaderModule;
    ASSERT(
        vkCreateShaderModule(_Context.GetDevice()->GetVKDevice(), &createInfo, nullptr, &shaderModule) == VK_SUCCESS,
        "Failed to create shader module!");

    return shaderModule;
}
