#include "Buffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Utils.h"
#include "VulkanContext.h"

Pipeline::Pipeline(VulkanContext& InContext, const PipelineBuildConfig& InConfig) : _Config(InConfig), _Context(InContext)
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
    // ------------------------------------------------------------------------
    // Shader stages
    // ------------------------------------------------------------------------
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    auto                                         loadShader = [&](const std::string& path, VkShaderStageFlagBits stage) -> VkShaderModule
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

    VkShaderModule vertShaderModule = loadShader(_Config.VertexShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderModule fragShaderModule = loadShader(_Config.FragmentShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkShaderModule geomShaderModule = loadShader(_Config.GeometryShaderPath, VK_SHADER_STAGE_GEOMETRY_BIT);

    // ------------------------------------------------------------------------
    // Vertex input
    // ------------------------------------------------------------------------
    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = static_cast<uint32_t>(_Config.VertexBindings.size());
    vertexInputInfo.pVertexBindingDescriptions      = _Config.VertexBindings.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(_Config.VertexAttributes.size());
    vertexInputInfo.pVertexAttributeDescriptions    = _Config.VertexAttributes.data();

    // ------------------------------------------------------------------------
    // Input assembly
    // ------------------------------------------------------------------------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = _Config.PrimitiveTopology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ------------------------------------------------------------------------
    // Viewport & scissor. If you're using dynamic states, these initial setups aren't important.
    // ------------------------------------------------------------------------
    VkViewport viewportDummy{};
    if (_Config.ViewportWidth == UINT32_MAX && _Config.ViewportHeight == UINT32_MAX)
    {
        viewportDummy.x        = 0.0f;
        viewportDummy.y        = 0.0f;
        viewportDummy.width    = (float)_Context.GetSurface()->GetVKExtent().width;
        viewportDummy.height   = (float)_Context.GetSurface()->GetVKExtent().height;
        viewportDummy.minDepth = 0.0f;
        viewportDummy.maxDepth = 1.0f;
    }
    else
    {
        viewportDummy.x        = 0.0f;
        viewportDummy.y        = 0.0f;
        viewportDummy.width    = (float)_Config.ViewportWidth;
        viewportDummy.height   = (float)_Config.ViewportHeight;
        viewportDummy.minDepth = 0.0f;
        viewportDummy.maxDepth = 1.0f;
    }

    VkRect2D scissorDummy{};
    scissorDummy.offset        = { 0, 0 };
    scissorDummy.extent.width  = _Config.ViewportWidth == UINT32_MAX ? _Context.GetSurface()->GetVKExtent().width : _Config.ViewportWidth;
    scissorDummy.extent.height = _Config.ViewportHeight == UINT32_MAX ? _Context.GetSurface()->GetVKExtent().height : _Config.ViewportHeight;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewportDummy;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissorDummy;

    // ------------------------------------------------------------------------
    // Rasterizer
    // ------------------------------------------------------------------------
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = _Config.PolygonMode;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = _Config.CullMode;
    rasterizer.frontFace               = _Config.FrontFace;
    rasterizer.depthBiasEnable         = _Config.EnableDepthBias;
    rasterizer.depthBiasConstantFactor = _Config.DepthBiasConstantFactor; // Optional
    rasterizer.depthBiasClamp          = _Config.DepthBiasClamp; // Optional
    rasterizer.depthBiasSlopeFactor    = _Config.DepthBiasSlopeFactor; // Optional

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
    colorBlending.pAttachments      = &_Config.ColorBlendAttachmentState;
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // ------------------------------------------------------------------------
    // Depth & stencil
    // ------------------------------------------------------------------------
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable       = _Config.EnableDepthTesting;
    depthStencil.depthWriteEnable      = _Config.EnableDepthWriting;
    depthStencil.depthCompareOp        = _Config.DepthCompareOp;
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
    dynamicState.dynamicStateCount = static_cast<uint32_t>(_Config.DynamicStates.size());
    dynamicState.pDynamicStates    = _Config.DynamicStates.data();

    // ------------------------------------------------------------------------
    // Pipeline layout
    // ------------------------------------------------------------------------
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts    = &_Config.DescriptorSetLayout->GetDescriptorLayout();

    if (_Config.PushConstantRanges.size() > 0)
    {
        // pushConstantRange.offset = _Config.PushConstantOffset;
        // pushConstantRange.size = _Config.PushConstantSize;
        // pushConstantRange.stageFlags = _Config.PushConstantShaderStage;
        pipelineLayoutInfo.pushConstantRangeCount = _Config.PushConstantRanges.size(); // Optional
        pipelineLayoutInfo.pPushConstantRanges    = _Config.PushConstantRanges.data(); // Optional
    }
    else
    {
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges    = nullptr; // Optional
    }

    ENSURE(vkCreatePipelineLayout(_Context.GetDevice()->GetVKDevice(), &pipelineLayoutInfo, nullptr, &_PipelineLayout) == VK_SUCCESS, "Failed to create pipeline layout");

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
    pipelineInfo.pDynamicState       = _Config.EnableDynamicStates ? &dynamicState : nullptr;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.layout              = _PipelineLayout;
    pipelineInfo.renderPass          = _Config.RenderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE; // Optional
    pipelineInfo.basePipelineIndex   = -1; // Optional

    ENSURE(
        vkCreateGraphicsPipelines(_Context.GetDevice()->GetVKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_Pipeline) == VK_SUCCESS,
        "Failed to create graphics pipeline!");

    // ------------------------------------------------------------------------
    // Cleanup temporary shader modules
    // ------------------------------------------------------------------------
    if (_Config.FragmentShaderPath != "None")
    {
        vkDestroyShaderModule(_Context.GetDevice()->GetVKDevice(), fragShaderModule, nullptr);
    }
    if (_Config.VertexShaderPath != "None")
    {
        vkDestroyShaderModule(_Context.GetDevice()->GetVKDevice(), vertShaderModule, nullptr);
    }
    if (_Config.GeometryShaderPath != "None")
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
    ENSURE(vkCreateShaderModule(_Context.GetDevice()->GetVKDevice(), &createInfo, nullptr, &shaderModule) == VK_SUCCESS, "Failed to create shader module!");

    return shaderModule;
}

PipelineBuilder::PipelineBuilder(VulkanContext& InContext) : _Context(InContext)
{
    _Config.PolygonMode             = VK_POLYGON_MODE_FILL;
    _Config.CullMode                = VK_CULL_MODE_BACK_BIT;
    _Config.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    _Config.EnableDepthBias         = VK_FALSE;
    _Config.EnableDepthTesting      = VK_TRUE;
    _Config.EnableDepthWriting      = VK_TRUE;
    _Config.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    _Config.DepthBiasConstantFactor = 0.0f;
    _Config.DepthBiasClamp          = 0.0f;
    _Config.DepthBiasSlopeFactor    = 0.0f;
    _Config.PrimitiveTopology       = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    _Config.ViewportWidth           = UINT32_MAX;
    _Config.ViewportHeight          = UINT32_MAX;
    _Config.VertexShaderPath        = "None";
    _Config.FragmentShaderPath      = "None";
    _Config.GeometryShaderPath      = "None";
    _Config.DynamicStates           = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    _Config.EnableDynamicStates     = true;

    // Default: standard alpha blending.
    _Config.ColorBlendAttachmentState                     = {};
    _Config.ColorBlendAttachmentState.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    _Config.ColorBlendAttachmentState.blendEnable         = VK_TRUE;
    _Config.ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    _Config.ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    _Config.ColorBlendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
    _Config.ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    _Config.ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    _Config.ColorBlendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
}

PipelineBuilder& PipelineBuilder::SetRenderPass(VkRenderPass InRenderPass)
{
    _Config.RenderPass = InRenderPass;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDescriptorSetLayout(Ref<DescriptorSetLayout> InLayout)
{
    _Config.DescriptorSetLayout = InLayout;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexShader(const std::string& InPath)
{
    _Config.VertexShaderPath = InPath;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetFragmentShader(const std::string& InPath)
{
    _Config.FragmentShaderPath = InPath;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetGeometryShader(const std::string& InPath)
{
    _Config.GeometryShaderPath = InPath;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexBindings(const std::vector<VkVertexInputBindingDescription>& InBindings)
{
    _Config.VertexBindings = InBindings;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetVertexAttributes(const std::vector<VkVertexInputAttributeDescription>& InAttributes)
{
    _Config.VertexAttributes = InAttributes;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthTest(VkBool32 InEnable)
{
    _Config.EnableDepthTesting = InEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthWrite(VkBool32 InEnable)
{
    _Config.EnableDepthWriting = InEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthCompareOp(VkCompareOp InOp)
{
    _Config.DepthCompareOp = InOp;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDepthBias(float InConstantFactor, float InClamp, float InSlopeFactor)
{
    _Config.EnableDepthBias         = VK_TRUE;
    _Config.DepthBiasConstantFactor = InConstantFactor;
    _Config.DepthBiasClamp          = InClamp;
    _Config.DepthBiasSlopeFactor    = InSlopeFactor;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetBlending(VkBool32 InEnable)
{
    _Config.ColorBlendAttachmentState.blendEnable = InEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetAlphaBlendFactors(VkBlendFactor InSrc, VkBlendFactor InDst)
{
    _Config.ColorBlendAttachmentState.srcAlphaBlendFactor = InSrc;
    _Config.ColorBlendAttachmentState.dstAlphaBlendFactor = InDst;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetCullMode(VkCullModeFlags InMode)
{
    _Config.CullMode = InMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetFrontFace(VkFrontFace InFace)
{
    _Config.FrontFace = InFace;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetPolygonMode(VkPolygonMode InMode)
{
    _Config.PolygonMode = InMode;
    return *this;
}

PipelineBuilder& PipelineBuilder::AddPushConstant(VkShaderStageFlags InStage, uint32_t InOffset, uint32_t InSize)
{
    VkPushConstantRange range{};
    range.stageFlags = InStage;
    range.offset     = InOffset;
    range.size       = InSize;
    _Config.PushConstantRanges.push_back(range);
    return *this;
}

PipelineBuilder& PipelineBuilder::SetTopology(VkPrimitiveTopology InTopology)
{
    _Config.PrimitiveTopology = InTopology;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetFixedViewport(uint32_t InWidth, uint32_t InHeight)
{
    _Config.ViewportWidth  = InWidth;
    _Config.ViewportHeight = InHeight;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDynamicStatesEnabled(bool InEnable)
{
    _Config.EnableDynamicStates = InEnable;
    return *this;
}

PipelineBuilder& PipelineBuilder::SetDynamicStates(const std::vector<VkDynamicState>& InDynamicStates)
{
    _Config.DynamicStates = InDynamicStates;
    return *this;
}

Ref<Pipeline> PipelineBuilder::Build()
{
    return make_s<Pipeline>(_Context, _Config);
}