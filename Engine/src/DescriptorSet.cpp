#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"

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
        vkCreateDescriptorPool(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &poolInfo, nullptr, &_DescriptorPool) == VK_SUCCESS,
        "Failed to create descriptor pool!");
}

DescriptorPool::~DescriptorPool()
{
    vkDestroyDescriptorPool(EngineInternal::GetContext().GetDevice()->GetVKDevice(), _DescriptorPool, nullptr);
}

const VkDescriptorPool& DescriptorPool::GetDescriptorPool() const
{
    return _DescriptorPool;
}

DescriptorSetLayout::DescriptorSetLayout(const std::vector<DescriptorBinding>& InBindings) : _Bindings(InBindings)
{
    std::vector<VkDescriptorSetLayoutBinding> vkBindings(InBindings.size());

    for (int i = 0; i < InBindings.size(); i++)
    {
        vkBindings[i].binding            = InBindings[i].Binding;
        vkBindings[i].descriptorType     = InBindings[i].Type;
        vkBindings[i].descriptorCount    = InBindings[i].Count;
        vkBindings[i].stageFlags         = InBindings[i].ShaderStage;
        vkBindings[i].pImmutableSamplers = nullptr;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
    layoutInfo.pBindings    = vkBindings.data();

    ENSURE(
        vkCreateDescriptorSetLayout(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &layoutInfo, nullptr, &_Layout) == VK_SUCCESS,
        "Failed to create descriptor set layout!");
}

DescriptorSetLayout::~DescriptorSetLayout()
{
    vkDestroyDescriptorSetLayout(EngineInternal::GetContext().GetDevice()->GetVKDevice(), _Layout, nullptr);
}

DescriptorSetLayout::DescriptorSetLayout(const VkDescriptorSetLayout& layout)
{
    _Layout = layout;
}

VkDescriptorSetLayout& DescriptorSetLayout::GetDescriptorLayout()
{
    return _Layout;
}
