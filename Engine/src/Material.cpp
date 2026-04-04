// Material.cpp
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Material.h"
#include "Pipeline.h"

Material::Material(const std::string& InName, Ref<Pipeline> InPipeline, Ref<DescriptorSetLayout> InLayout) : _Name(InName), _Pipeline(InPipeline), _Layout(InLayout)
{
}

void Material::Bind(VkCommandBuffer InCmd)
{
    CommandBuffer::BindPipeline(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _Pipeline);
}

Ref<Pipeline> Material::GetPipeline() const
{
    return _Pipeline;
}

Ref<DescriptorSetLayout> Material::GetLayout() const
{
    return _Layout;
}

VkPipelineLayout Material::GetPipelineLayout() const
{
    return _Pipeline->GetPipelineLayout();
}

const std::string& Material::GetName() const
{
    return _Name;
}