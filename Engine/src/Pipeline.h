#pragma once
#include "core.h"

// External
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
class DescriptorSetLayout;
class VulkanContext;
class Pipeline
{
   public:
    struct Specs
    {
        VkRenderPass                                   RenderPass;
        Ref<DescriptorSetLayout>                       DescriptorSetLayout;
        std::string                                    VertexShaderPath   = "None";
        std::string                                    FragmentShaderPath = "None";
        std::string                                    GeometryShaderPath = "None";
        VkPolygonMode                                  PolygonMode;
        VkCullModeFlags                                CullMode;
        VkFrontFace                                    FrontFace;
        VkBool32                                       EnableDepthBias;
        float                                          DepthBiasConstantFactor;
        float                                          DepthBiasClamp;
        float                                          DepthBiasSlopeFactor;
        VkBool32                                       EnableDepthTesting;
        VkBool32                                       EnableDepthWriting;
        VkCompareOp                                    DepthCompareOp;
        uint32_t                                       ViewportWidth  = UINT32_MAX;
        uint32_t                                       ViewportHeight = UINT32_MAX;
        std::vector<VkPushConstantRange>               PushConstantRanges;
        VkPrimitiveTopology                            PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineColorBlendAttachmentState            ColorBlendAttachmentState;
        std::vector<VkVertexInputBindingDescription>   VertexBindings   = {};
        std::vector<VkVertexInputAttributeDescription> VertexAttributes = {};
    };

   public:
    Pipeline(VulkanContext& InContext, const Specs& InSpecs);
    ~Pipeline();
    void Resize();

    VkPipeline       GetHandle() const;
    VkPipelineLayout GetPipelineLayout() const;

   private:
    void           Init();
    void           Cleanup();
    VkShaderModule CreateShaderModule(const std::vector<char>& InShaderCode);

   private:
    VkPipeline                  _Pipeline;
    VkPipelineLayout            _PipelineLayout;
    std::vector<VkDynamicState> _DynamicStates;
    Specs                       _Specs;
    VulkanContext&              _Context;
};
