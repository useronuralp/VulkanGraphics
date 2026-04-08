#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

class Pipeline;
struct DescriptorBinding
{
    uint32_t           Binding;
    VkDescriptorType   Type;
    uint32_t           Count = 1;
    VkShaderStageFlags ShaderStage;
};

class DescriptorSetLayout
{
   public:
    DescriptorSetLayout() = default;
    DescriptorSetLayout(const std::vector<DescriptorBinding>& InBindings);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const VkDescriptorSetLayout& layout);
    VkDescriptorSetLayout& GetDescriptorLayout();

   private:
    VkDescriptorSetLayout          _Layout = VK_NULL_HANDLE;
    std::vector<DescriptorBinding> _Bindings;
};

class DescriptorPool
{
   public:
    DescriptorPool() = default;
    DescriptorPool(const VkDescriptorPool& pool) : _DescriptorPool(pool) {};
    DescriptorPool(uint32_t maximumDescriptorCount, std::vector<VkDescriptorType> dscTypes);
    ~DescriptorPool();

    const VkDescriptorPool& GetDescriptorPool() const;

   private:
    VkDescriptorPool _DescriptorPool = VK_NULL_HANDLE;
};
