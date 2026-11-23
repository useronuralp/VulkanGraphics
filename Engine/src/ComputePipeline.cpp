#include "ComputePipeline.h"

#include "DescriptorSet.h"
#include "Device.h"
#include "Utils.h"
#include "VulkanContext.h"

ComputePipeline::ComputePipeline(VulkanContext& InContext, const ComputePipelineDesc& InDesc)
    : _Device(InContext.GetDevice()->GetVKDevice()), _PushConstantSize(InDesc.PushConstantSize), _GroupSize(InDesc.GroupSize)
{
    ENSURE(_GroupSize.x > 0 && _GroupSize.y > 0, "Compute group size must be non-zero.");

    // ── Layout: which descriptor sets and push constants the shader can see ──
    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.reserve(InDesc.SetLayouts.size());
    for (const auto& layout : InDesc.SetLayouts)
    {
        setLayouts.push_back(layout->GetDescriptorLayout());
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = _PushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts            = setLayouts.data();
    layoutInfo.pushConstantRangeCount = _PushConstantSize > 0 ? 1u : 0u;
    layoutInfo.pPushConstantRanges    = _PushConstantSize > 0 ? &pushRange : nullptr;
    ENSURE(vkCreatePipelineLayout(_Device, &layoutInfo, nullptr, &_Layout) == VK_SUCCESS, "Failed to create compute pipeline layout for " + InDesc.ShaderPath);

    // ── Shader module: only needed while the pipeline is being compiled ──
    const std::vector<char> code = Utils::ReadFile(InDesc.ShaderPath);

    VkShaderModuleCreateInfo moduleInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    moduleInfo.codeSize = code.size();
    moduleInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    ENSURE(vkCreateShaderModule(_Device, &moduleInfo, nullptr, &shaderModule) == VK_SUCCESS, "Failed to create shader module for " + InDesc.ShaderPath);

    // ── Pipeline ──
    VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.stage        = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    pipelineInfo.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName  = "main";
    pipelineInfo.layout       = _Layout;
    ENSURE(
        vkCreateComputePipelines(_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_Pipeline) == VK_SUCCESS,
        "Failed to create compute pipeline for " + InDesc.ShaderPath);

    // The pipeline holds its own compiled copy.
    vkDestroyShaderModule(_Device, shaderModule, nullptr);
}

ComputePipeline::~ComputePipeline()
{
    // Caller guarantees the GPU is done with it (renderers wait for idle in Cleanup / resize).
    vkDestroyPipeline(_Device, _Pipeline, nullptr);
    vkDestroyPipelineLayout(_Device, _Layout, nullptr);
}

void ComputePipeline::Bind(VkCommandBuffer InCmd) const
{
    vkCmdBindPipeline(InCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _Pipeline);
}

void ComputePipeline::BindDescriptorSet(VkCommandBuffer InCmd, VkDescriptorSet InSet, uint32_t InSetIndex) const
{
    // Compute and graphics keep separate descriptor bindings, so a set bound for the display pass
    // isn't visible here until it's bound again at the COMPUTE bind point.
    vkCmdBindDescriptorSets(InCmd, VK_PIPELINE_BIND_POINT_COMPUTE, _Layout, InSetIndex, 1, &InSet, 0, nullptr);
}

void ComputePipeline::PushConstants(VkCommandBuffer InCmd, const void* InData, uint32_t InSize) const
{
    ENSURE(InSize <= _PushConstantSize, "Push constant data is larger than the range declared for this pipeline.");
    vkCmdPushConstants(InCmd, _Layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, InSize, InData);
}

void ComputePipeline::DispatchForExtent(VkCommandBuffer InCmd, uint32_t InWidth, uint32_t InHeight) const
{
    const uint32_t groupsX = (InWidth + _GroupSize.x - 1) / _GroupSize.x;
    const uint32_t groupsY = (InHeight + _GroupSize.y - 1) / _GroupSize.y;
    vkCmdDispatch(InCmd, groupsX, groupsY, 1);
}
