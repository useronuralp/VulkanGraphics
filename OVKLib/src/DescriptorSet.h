#pragma once
#include "core.h"
// External
#include <vulkan/vulkan.h>
#include <vector>
namespace OVK
{

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
	};
	enum class ShaderStage
	{
		FRAGMENT,
		VERTEX
	};
	struct DescriptorBindingSpecs
	{
		Type		Type;
		size_t		Size;
		int			Count;
		ShaderStage ShaderStage;
		uint32_t	Binding;
	};
	class Pipeline;
	class DescriptorLayout
	{
	public:
		DescriptorLayout() = default;
		DescriptorLayout(const std::vector<DescriptorBindingSpecs>& layout);
		DescriptorLayout(const VkDescriptorSetLayout& layout) { m_DescriptorSetLayout = layout; }
		~DescriptorLayout();
		const std::vector<DescriptorBindingSpecs>& GetBindingSpecs() { return m_SetLayout; }
		const VkDescriptorSetLayout& GetDescriptorLayout() { return m_DescriptorSetLayout; }
	private:
		VkDescriptorSetLayout m_DescriptorSetLayout = VK_NULL_HANDLE;
		std::vector<DescriptorBindingSpecs>    m_SetLayout;
	};
	class DescriptorPool
	{
	public:
		DescriptorPool() = default;
		DescriptorPool(const VkDescriptorPool& pool) { m_DescriptorPool = pool; }
		DescriptorPool(uint32_t maximumDescriptorCount, std::vector<VkDescriptorType> dscTypes);
		~DescriptorPool();
		const VkDescriptorPool& GetDescriptorPool() const { return m_DescriptorPool; }
	private:
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
	};
	class DescriptorSet
	{
	public:
		DescriptorSet() = default;
		DescriptorSet(const std::vector<DescriptorBindingSpecs>& layout);
		DescriptorSet(const DescriptorPool& poolToAllocateFrom, DescriptorLayout& layout);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() const { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() const { return m_DescriptorSetLayout; }
		const std::vector<DescriptorBindingSpecs>& GetLayout() const { return m_SetLayout; }
	private:
		VkDescriptorSet					    m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool				    m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout			    m_DescriptorSetLayout = VK_NULL_HANDLE;
		std::vector<DescriptorBindingSpecs>    m_SetLayout;
	};

	static VkShaderStageFlags FromShaderStageToDescriptorType(ShaderStage stage)
	{
		switch (stage)
		{
			case ShaderStage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
			case ShaderStage::VERTEX:	return VK_SHADER_STAGE_VERTEX_BIT;
		}
	}
}