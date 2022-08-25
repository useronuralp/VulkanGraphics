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
		MAT4,
		VEC3,
		VEC2
	};
	enum class ShaderStage
	{
		FRAGMENT,
		VERTEX
	};
	struct DescriptorLayout
	{
		Type		Type;
		Size		Size;
		ShaderStage ShaderStage;
	};
	class Pipeline;
	class DescriptorSet
	{
	public:
		DescriptorSet(const std::vector<DescriptorLayout>& layout);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
		const std::vector<DescriptorLayout>& GetShaderLayout() { return m_ShaderLayout; }
	private:
		VkDescriptorSet					 m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool				 m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout			 m_DescriptorSetLayout = VK_NULL_HANDLE;
		std::vector<DescriptorLayout>  m_ShaderLayout;
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