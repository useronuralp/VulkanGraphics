#include "Pipeline.h"
#include "VulkanApplication.h"
#include "Utils.h"
#include "LogicalDevice.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Surface.h"
#include <iostream>
namespace Anor
{
    Pipeline::Pipeline(const Ref<DescriptorSet>& dscSet)
        : m_DescriptorSet(dscSet)
	{
        Init(m_DescriptorSet);
	}
    Pipeline::~Pipeline()
    {
        Cleanup();
    }
    void Pipeline::OnResize()
    {
        Cleanup();
        Init(m_DescriptorSet); //Re initializes the pipeline with correct framebuffer dimensions.
    }
    void Pipeline::Cleanup()
    {
        vkDestroyPipelineLayout(VulkanApplication::s_Device->GetVKDevice(), m_PipelineLayout, nullptr);
        vkDestroyPipeline(VulkanApplication::s_Device->GetVKDevice(), m_Pipeline, nullptr);
    }
    void Pipeline::Init(const Ref<DescriptorSet>& dscSet)
    {
        m_DynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH
        };

        // Read shaders from file.
        auto vertShaderCode = Utils::ReadFile("shaders/vert.spv");
        auto fragShaderCode = Utils::ReadFile("shaders/frag.spv");


        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        // Vertex shader stage info.
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // Fragment shader stage info.
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        // This "VkPipelineVertexInputStateCreateInfo" strcut specifies the format of the data that you are going to pass to the vertex shader.
        // Bindings: Spacing between data and whether the data is per-vertex or per-instance.
        // Attribute: Type of the attributes passed to the vertex shader, which binding to load them from and at which offset.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        // This struct assembels the points and forms whatever primitive you want to form. These primitives are usually: triangles, lines and points.
        // We are interested in drawing trianlgles in our case.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport specifies the dimensions of the framebuffer that we want to draw to. This usually spans the entirety of a framebuffer.
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)VulkanApplication::s_Surface->GetVKExtent().width;
        viewport.height = (float)VulkanApplication::s_Surface->GetVKExtent().height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Any pixel outside of the scissor boundaries gets discarded. Scissors act rather as filters and required to be specified.
        // If you do not want any kind of discarding due to scissors, make its extent the same as the framebuffer / swap chain images.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = VulkanApplication::s_Surface->GetVKExtent();

        // The above two states "VkRect2D" and "VkViewport" need to be combined into a single struct called "VkPipelineViewportStateCreateInfo". That is 
        // exactly what we are doing here.
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;


        // This structure configures the rasterizer.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // Multisampling configuration here.
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{}; // Empty for now.

        // Color blebding configuration. This blends the current color that the fragment shader returns with that is already present in the framebuffer with the 
        // parameters specified below. The below setting is usually what you need most of the time.
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // Depth & Stencil testing configuration
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        // Dynamic states that can be changed during drawing time. The states that can be found in "dynamicStates" can be changed without recreateing the pipeline.
        // Viewport size (screen resizing) is a good example for this.
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
        dynamicState.pDynamicStates = m_DynamicStates.data();

        // This struct specifies how the "UNIFORM" variables are going to be laid out. TO DO: Learn it better.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSet->GetVKDescriptorSetLayout();

        //// TEMP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        //VkPushConstantRange PC;
        //PC.offset = 0;
        //PC.size = (uint32_t)sizeof(glm::vec4);
        //PC.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        //// TEMP!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        ASSERT(vkCreatePipelineLayout(VulkanApplication::s_Device->GetVKDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS, "Failed to create pipeline layout");

        // This struct binds together all the other structs we have created so far in this function up above.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional
        pipelineInfo.pDepthStencilState = &depthStencil;

        pipelineInfo.layout = m_PipelineLayout;

        pipelineInfo.renderPass = VulkanApplication::s_ModelRenderPass->GetRenderPass();
        pipelineInfo.subpass = 0;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        ASSERT(vkCreateGraphicsPipelines(VulkanApplication::s_Device->GetVKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) == VK_SUCCESS, "Failed to create graphics pipeline!");

        vkDestroyShaderModule(VulkanApplication::s_Device->GetVKDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(VulkanApplication::s_Device->GetVKDevice(), vertShaderModule, nullptr);
    }
    VkShaderModule Pipeline::CreateShaderModule(const std::vector<char>& shaderCode)
    {
        VkShaderModuleCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule;
        ASSERT(vkCreateShaderModule(VulkanApplication::s_Device->GetVKDevice(), &createInfo, nullptr, &shaderModule) == VK_SUCCESS, "Failed to create shader module!");

        return shaderModule;
    }


}