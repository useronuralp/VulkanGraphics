#pragma once
#include "core.h"
// External
#include <vector>
#include <vulkan/vulkan.h>
enum class Type
{
    TEXTURE_SAMPLER_AMBIENTOCCLUSION,
    TEXTURE_SAMPLER_SHADOWMAP,
    TEXTURE_SAMPLER_METALLIC,
    TEXTURE_SAMPLER_DIFFUSE,
    TEXTURE_SAMPLER_SPECULAR,
    TEXTURE_SAMPLER_NORMAL,
    TEXTURE_SAMPLER_ROUGHNESSMETALLIC,
    TEXTURE_SAMPLER_CUBEMAP,
    UNIFORM_BUFFER,
    TEXTURE_SAMPLER_POINTSHADOWMAP
};
struct DescriptorSetBindingSpecs
{
    Type               Type;
    size_t             Size; // todo: this is unused. Remove later.
    int                Count;
    VkShaderStageFlags ShaderStage;
    uint32_t           Binding;
};
class Pipeline;
class DescriptorSetLayout
{
   public:
    DescriptorSetLayout() = default;
    DescriptorSetLayout(const std::vector<DescriptorSetBindingSpecs>& layout);
    ~DescriptorSetLayout();
    DescriptorSetLayout(const VkDescriptorSetLayout& layout);
    std::vector<DescriptorSetBindingSpecs>& GetBindingSpecs();
    VkDescriptorSetLayout&                  GetDescriptorLayout();

   private:
    VkDescriptorSetLayout                  m_DescriptorSetLayout = VK_NULL_HANDLE;
    std::vector<DescriptorSetBindingSpecs> m_SetLayout;
};
class DescriptorPool
{
   public:
    DescriptorPool() = default;
    DescriptorPool(const VkDescriptorPool& pool)
    {
        m_DescriptorPool = pool;
    }
    DescriptorPool(uint32_t maximumDescriptorCount, std::vector<VkDescriptorType> dscTypes);
    ~DescriptorPool();
    const VkDescriptorPool& GetDescriptorPool() const
    {
        return m_DescriptorPool;
    }

   private:
    VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
};
