#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <vector>
namespace Anor
{

	enum class Type
	{
		TEXTURE_SAMPLER,
		UNIFORM_BUFFER,
	};
	enum class Size
	{
		MVP_UNIFORM_BUFFER,
		DIFFUSE_SAMPLER,
		SPECULAR_SAMPLER,
		NORMAL_SAMPLER,
		ROUGHNESS_SAMPLER,
		METALLIC_SAMPLER,
		ROUGHNESS_METALLIC_SAMPLER,
		AMBIENT_OCCLUSION_SAMPLER,
		SHADOWMAP_SAMPLER,
		CUBEMAP_SAMPLER,
		FLOAT,
		MAT4,
		VEC4,
		VEC3,
		VEC2
	};
	enum class ShaderStage
	{
		FRAGMENT,
		VERTEX
	};
	struct DescriptorSetLayout
	{
		Type		Type;
		Size		Size;
		int			Count;
		ShaderStage ShaderStage;
	};
	class Pipeline;
	class DescriptorSet
	{
	public:
		DescriptorSet(const std::vector<DescriptorSetLayout>& layout);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
		const std::vector<DescriptorSetLayout>& GetLayout() { return m_SetLayout; }
	private:
		VkDescriptorSet					    m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool				    m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout			    m_DescriptorSetLayout = VK_NULL_HANDLE;
		std::vector<DescriptorSetLayout>    m_SetLayout;
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