#pragma once
#include "core.h"

#include <type_traits>
#include <vulkan/vulkan.h>

class DescriptorSetLayout;
class VulkanContext;

// What a compute pipeline is made of. Designated-initialiser friendly:
//   ComputePipeline(ctx, { .ShaderPath = "...", .SetLayouts = { layout }, .PushConstantSize = sizeof(Params) });
struct ComputePipelineDesc
{
    std::string                           ShaderPath;           // relative to Engine/, same convention as PipelineBuilder
    std::vector<Ref<DescriptorSetLayout>> SetLayouts;           // set = 0, 1, ... in declaration order
    uint32_t                              PushConstantSize = 0; // bytes, 0 = none. 128 is the guaranteed minimum limit.
    glm::uvec2                            GroupSize{ 8, 8 };    // must match local_size_x / local_size_y in the shader
};

// -------------------------------------------------------------------------------------------------
// ComputePipeline
// -------------------------------------------------------------------------------------------------
// PipelineBuilder only makes graphics pipelines. A compute pipeline is much smaller: one shader stage
// plus a pipeline layout (descriptor set layouts + push constant range). No vertex input, rasterizer,
// blend state or render pass. This wraps creation, destruction and the per-frame recording calls.
// -------------------------------------------------------------------------------------------------
class ComputePipeline
{
   public:
    ComputePipeline(VulkanContext& InContext, const ComputePipelineDesc& InDesc);
    ~ComputePipeline();

    ComputePipeline(const ComputePipeline&)            = delete;
    ComputePipeline& operator=(const ComputePipeline&) = delete;

    void Bind(VkCommandBuffer InCmd) const;
    void BindDescriptorSet(VkCommandBuffer InCmd, VkDescriptorSet InSet, uint32_t InSetIndex = 0) const;

    template <typename T>
    void PushConstants(VkCommandBuffer InCmd, const T& InData) const
    {
        static_assert(std::is_trivially_copyable_v<T>, "Push constants are copied byte for byte.");
        PushConstants(InCmd, &InData, sizeof(T));
    }
    void PushConstants(VkCommandBuffer InCmd, const void* InData, uint32_t InSize) const;

    // One invocation per pixel: rounds the group count up, so the shader must bounds-check the edges.
    void DispatchForExtent(VkCommandBuffer InCmd, uint32_t InWidth, uint32_t InHeight) const;

    VkPipeline GetHandle() const
    {
        return _Pipeline;
    }
    VkPipelineLayout GetLayout() const
    {
        return _Layout;
    }

   private:
    VkDevice         _Device           = VK_NULL_HANDLE;
    VkPipelineLayout _Layout           = VK_NULL_HANDLE;
    VkPipeline       _Pipeline         = VK_NULL_HANDLE;
    uint32_t         _PushConstantSize = 0;
    glm::uvec2       _GroupSize{ 8, 8 };
};
