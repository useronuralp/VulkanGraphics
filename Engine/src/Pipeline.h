#pragma once
#include "core.h"

// External
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
class DescriptorSetLayout;
class VulkanContext;

struct PipelineBuildConfig
{
    VkRenderPass                                   RenderPass          = VK_NULL_HANDLE;
    Ref<DescriptorSetLayout>                       DescriptorSetLayout = nullptr;
    std::string                                    VertexShaderPath;
    std::string                                    FragmentShaderPath;
    std::string                                    GeometryShaderPath;
    VkPolygonMode                                  PolygonMode             = VK_POLYGON_MODE_FILL;
    VkCullModeFlags                                CullMode                = VK_CULL_MODE_NONE;
    VkFrontFace                                    FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    VkBool32                                       EnableDepthBias         = VK_FALSE;
    float                                          DepthBiasConstantFactor = 0.0f;
    float                                          DepthBiasClamp          = 0.0f;
    float                                          DepthBiasSlopeFactor    = 0.0f;
    VkBool32                                       EnableDepthTesting      = VK_FALSE;
    VkBool32                                       EnableDepthWriting      = VK_FALSE;
    VkCompareOp                                    DepthCompareOp          = VK_COMPARE_OP_NEVER;
    uint32_t                                       ViewportWidth           = UINT32_MAX;
    uint32_t                                       ViewportHeight          = UINT32_MAX;
    std::vector<VkPushConstantRange>               PushConstantRanges;
    VkPrimitiveTopology                            PrimitiveTopology         = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineColorBlendAttachmentState            ColorBlendAttachmentState = {};
    std::vector<VkVertexInputBindingDescription>   VertexBindings;
    std::vector<VkVertexInputAttributeDescription> VertexAttributes;
    std::vector<VkDynamicState>                    DynamicStates;
    bool                                           EnableDynamicStates = false;
};

class Pipeline
{
   public:
   public:
    Pipeline(VulkanContext& InContext, const PipelineBuildConfig& InConfig);
    ~Pipeline();
    void Resize();

    VkPipeline       GetHandle() const;
    VkPipelineLayout GetPipelineLayout() const;

   private:
    void           Init();
    void           Cleanup();
    VkShaderModule CreateShaderModule(const std::vector<char>& InShaderCode);

   private:
    VkPipeline          _Pipeline;
    VkPipelineLayout    _PipelineLayout;
    PipelineBuildConfig _Config;
    VulkanContext&      _Context;
};

class PipelineBuilder
{
   public:
    explicit PipelineBuilder(VulkanContext& InContext);

    // Required — no defaults for these.
    PipelineBuilder& SetRenderPass(VkRenderPass InRenderPass);
    PipelineBuilder& SetDescriptorSetLayout(Ref<DescriptorSetLayout> InLayout);
    PipelineBuilder& SetVertexShader(const std::string& InPath);
    PipelineBuilder& SetFragmentShader(const std::string& InPath);
    PipelineBuilder& SetGeometryShader(const std::string& InPath);

    // Not required - defaults provided for these, but you can override them if you want.
    PipelineBuilder& SetVertexBindings(const std::vector<VkVertexInputBindingDescription>& InBindings);
    PipelineBuilder& SetVertexAttributes(const std::vector<VkVertexInputAttributeDescription>& InAttributes);

    PipelineBuilder& SetDepthTest(VkBool32 InEnable);
    PipelineBuilder& SetDepthWrite(VkBool32 InEnable);
    PipelineBuilder& SetDepthCompareOp(VkCompareOp InOp);
    PipelineBuilder& SetDepthBias(float InConstantFactor, float InClamp, float InSlopeFactor);

    PipelineBuilder& SetBlending(VkBool32 InEnable);
    PipelineBuilder& SetAlphaBlendFactors(VkBlendFactor InSrc, VkBlendFactor InDst);

    PipelineBuilder& SetCullMode(VkCullModeFlags InMode);
    PipelineBuilder& SetFrontFace(VkFrontFace InFace);
    PipelineBuilder& SetPolygonMode(VkPolygonMode InMode);

    PipelineBuilder& AddPushConstant(VkShaderStageFlags InStage, uint32_t InOffset, uint32_t InSize);
    PipelineBuilder& SetTopology(VkPrimitiveTopology InTopology);

    PipelineBuilder& SetFixedViewport(uint32_t InWidth, uint32_t InHeight);
    PipelineBuilder& SetDynamicStatesEnabled(bool InEnable);
    PipelineBuilder& SetDynamicStates(const std::vector<VkDynamicState>& InDynamicStates);

    Ref<Pipeline> Build();

   private:
    VulkanContext&      _Context;
    PipelineBuildConfig _Config;
};
