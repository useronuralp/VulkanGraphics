#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Pipeline.h"
#include "VulkanContext.h"

#include <iostream>

DescriptorPool::DescriptorPool(uint32_t maximumDescriptorCount, std::vector<VkDescriptorType> types)
{
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.resize(types.size());
    for (int i = 0; i < types.size(); i++)
    {
        poolSizes[i].type            = types[i];
        poolSizes[i].descriptorCount = 50; // TO DO: What does this variable do?
    }
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();
    poolInfo.maxSets       = maximumDescriptorCount; // Increase this value as you reach the limit of
                                               // allocations or just reallocate pools.
    // poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    ENSURE(
        vkCreateDescriptorPool(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &poolInfo, nullptr, &m_DescriptorPool) == VK_SUCCESS,
        "Failed to create descriptor pool!");
}
DescriptorPool::~DescriptorPool()
{
    vkDestroyDescriptorPool(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_DescriptorPool, nullptr);
}
DescriptorSetLayout::DescriptorSetLayout(const std::vector<DescriptorSetBindingSpecs>& layout)
{
    m_SetLayout = layout;
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    bindings.resize(layout.size());

    for (int i = 0; i < layout.size(); i++)
    {
        bindings[i].binding            = layout[i].Binding;
        bindings[i].descriptorCount    = layout[i].Count;
        bindings[i].pImmutableSamplers = nullptr;
        bindings[i].stageFlags         = layout[i].ShaderStage;

        if (layout[i].Type == Type::UNIFORM_BUFFER)
        {
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else
        {
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    ENSURE(
        vkCreateDescriptorSetLayout(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) == VK_SUCCESS,
        "Failed to create descriptor set layout!");
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_DescriptorSetLayout, nullptr);
}

DescriptorSetLayout::DescriptorSetLayout(const VkDescriptorSetLayout& layout)
{
    m_DescriptorSetLayout = layout;
}
std::vector<DescriptorSetBindingSpecs>& DescriptorSetLayout::GetBindingSpecs()
{
    return m_SetLayout;
}
VkDescriptorSetLayout& DescriptorSetLayout::GetDescriptorLayout()
{
    return m_DescriptorSetLayout;
}
