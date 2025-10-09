#include "Pipeline.h"
#include "VulkanApplication.h"
#include "Utils.h"
#include "LogicalDevice.h"
#include "Swapchain.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Surface.h"
#include <iostream>
#include "DescriptorSet.h"
namespace OVK
{
    Pipeline::Pipeline(const Specs& CI)
        : m_CI(CI)
	{
        Init();
	}
    Pipeline::~Pipeline()
    {
        Cleanup();
    }
    void Pipeline::ReConstruct()
    {
        Cleanup();
        Init(); //Re initializes the pipeline with correct framebuffer dimensions.
    }
    void Pipeline::Cleanup()
    {
        vkDestroyPipelineLayout(VulkanApplication::s_Device->GetVKDevice(), m_PipelineLayout, nullptr);
        vkDestroyPipeline(VulkanApplication::s_Device->GetVKDevice(), m_Pipeline, nullptr);
    }
    void Pipeline::Init()
    {
        m_DynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
            VK_DYNAMIC_STATE_SCISSOR
        };

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;


        std::vector<char> vertShaderCode;
        std::vector<char> fragShaderCode;
        std::vector<char> geomShaderCode;

        VkShaderModule vertShaderModule = VK_NULL_HANDLE;
        VkShaderModule fragShaderModule = VK_NULL_HANDLE;
        VkShaderModule geomShaderModule = VK_NULL_HANDLE;
        // Read shaders from file.
        if (m_CI.VertexShaderPath != "None")
        {
            vertShaderCode = Utils::ReadFile(m_CI.VertexShaderPath);
            vertShaderModule = CreateShaderModule(vertShaderCode);

            // Vertex shader stage info.
            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main"; // Shaders can have multiple entry points. Input the name of the entry point you want.

            shaderStages.push_back(vertShaderStageInfo);
        }
        if (m_CI.FragmentShaderPath!= "None")
        {
            fragShaderCode = Utils::ReadFile(m_CI.FragmentShaderPath);
            fragShaderModule = CreateShaderModule(fragShaderCode);

            // Fragment shader stage info.
            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            shaderStages.push_back(fragShaderStageInfo);
        }

        if (m_CI.GeometryShaderPath != "None")
        {
            geomShaderCode = Utils::ReadFile(m_CI.GeometryShaderPath);
            geomShaderModule = CreateShaderModule(geomShaderCode);

            // Fragment shader stage info.
            VkPipelineShaderStageCreateInfo geomShaderStageInfo{};
            geomShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            geomShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            geomShaderStageInfo.module = geomShaderModule;
            geomShaderStageInfo.pName = "main";

            shaderStages.push_back(geomShaderStageInfo);
        }






        // This "VkPipelineVertexInputStateCreateInfo" strcut specifies the format of the data that you are going to pass to the vertex shader.
        // Bindings: Spacing between data and whether the data is per-vertex or per-instance.
        // Attribute: Type of the attributes passed to the vertex shader, which binding to load them from and at which offset.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = m_CI.VertexInputBindingCount;
        vertexInputInfo.pVertexBindingDescriptions = m_CI.pVertexInputBindingDescriptions;
        vertexInputInfo.vertexAttributeDescriptionCount = m_CI.VertexInputAttributeCount;
        vertexInputInfo.pVertexAttributeDescriptions = m_CI.pVertexInputAttributeDescriptons;

        // This struct assembels the points and forms whatever primitive you want to form. These primitives are usually: triangles, lines and points.
        // We are interested in drawing trianlgles in our case.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = m_CI.PrimitiveTopology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport specifies the dimensions of the framebuffer that we want to draw to. This usually spans the entirety of a framebuffer.
        VkViewport viewport{};
        if (m_CI.ViewportWidth == UINT32_MAX && m_CI.ViewportHeight == UINT32_MAX)
        {
            viewport.x = 0.0f;
            viewport.y = (float)VulkanApplication::s_Surface->GetVKExtent().height;
            viewport.width = (float)VulkanApplication::s_Surface->GetVKExtent().width;
            viewport.height = -(float)VulkanApplication::s_Surface->GetVKExtent().height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
        }
        else
        {
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)m_CI.ViewportWidth;
            viewport.height = (float)m_CI.ViewportHeight;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
        }

        // Any pixel outside of the scissor boundaries gets discarded. Scissors act rather as filters and required to be specified.
        // If you do not want any kind of discarding due to scissors, make its extent the same as the framebuffer / swap chain images.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent.width = m_CI.ViewportWidth == UINT32_MAX ? VulkanApplication::s_Surface->GetVKExtent().width : m_CI.ViewportWidth ;
        scissor.extent.height = m_CI.ViewportHeight == UINT32_MAX ? VulkanApplication::s_Surface->GetVKExtent().height : m_CI.ViewportHeight;

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
        rasterizer.polygonMode = m_CI.PolygonMode;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = m_CI.CullMode;
        rasterizer.frontFace = m_CI.FrontFace;
        rasterizer.depthBiasEnable = m_CI.EnableDepthBias;
        rasterizer.depthBiasConstantFactor = m_CI.DepthBiasConstantFactor; // Optional
        rasterizer.depthBiasClamp = m_CI.DepthBiasClamp; // Optional
        rasterizer.depthBiasSlopeFactor = m_CI.DepthBiasSlopeFactor; // Optional

        // Multisampling configuration here.
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &m_CI.ColorBlendAttachmentState;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // Depth & Stencil testing configuration
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = m_CI.EnableDepthTesting;
        depthStencil.depthWriteEnable = m_CI.EnableDepthWriting;
        depthStencil.depthCompareOp = m_CI.DepthCompareOp;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS; // Optional

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
        pipelineLayoutInfo.pSetLayouts = &m_CI.DescriptorSetLayout->GetDescriptorLayout();

        if (m_CI.PushConstantRanges.size() > 0)
        {
            //pushConstantRange.offset = m_CI.PushConstantOffset;
            //pushConstantRange.size = m_CI.PushConstantSize;
            //pushConstantRange.stageFlags = m_CI.PushConstantShaderStage;
            pipelineLayoutInfo.pushConstantRangeCount = m_CI.PushConstantRanges.size(); // Optional
            pipelineLayoutInfo.pPushConstantRanges = m_CI.PushConstantRanges.data(); // Optional
        }
        else
        {
            pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
        }

        ASSERT(vkCreatePipelineLayout(VulkanApplication::s_Device->GetVKDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) == VK_SUCCESS, "Failed to create pipeline layout");

        // This struct binds together all the other structs we have created so far in this function up above.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
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
        pipelineInfo.renderPass = *m_CI.pRenderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        ASSERT(vkCreateGraphicsPipelines(VulkanApplication::s_Device->GetVKDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) == VK_SUCCESS, "Failed to create graphics pipeline!");

        if (m_CI.FragmentShaderPath != "None")
        {
            vkDestroyShaderModule(VulkanApplication::s_Device->GetVKDevice(), fragShaderModule, nullptr);
        }
        if (m_CI.VertexShaderPath!= "None")
        {
            vkDestroyShaderModule(VulkanApplication::s_Device->GetVKDevice(), vertShaderModule, nullptr);
        }
        if (m_CI.GeometryShaderPath != "None")
        {
            vkDestroyShaderModule(VulkanApplication::s_Device->GetVKDevice(), geomShaderModule, nullptr);
        }
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