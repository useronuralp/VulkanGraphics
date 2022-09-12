#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
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
	class DescriptorSet
	{
	public:
		DescriptorSet(const std::vector<DescriptorBindingSpecs>& layout);
		void WriteDescriptorSet(uint32_t bindingIndex, const VkDescriptorBufferInfo& bufferInfo, const VkDescriptorImageInfo& imageInfo);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
		const std::vector<DescriptorBindingSpecs>& GetLayout() { return m_SetLayout; }
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