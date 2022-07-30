#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <vector>
namespace Anor
{

	enum class ShaderBindingType
	{
		TEXTURE_SAMPLER,
		UNIFORM_BUFFER,
	};
	enum class UniformUsageType
	{
		MVP_UNIFORM_BUFFER,
		DIFFUSE_SAMPLER,
		SPECULAR_SAMPLER,
		NORMAL_SAMPLER,
		ROUGHNESS_SAMPLER,
		METALLIC_SAMPLER,
		ROUGHNESS_METALLIC_SAMPLER,
		AMBIENT_OCCLUSION_SAMPLER,
		MAT4,
		VEC3,
	};
	enum class ShaderStage
	{
		FRAGMENT,
		VERTEX
	};
	struct ShaderBindingSpecs
	{
		ShaderBindingType Type;
		UniformUsageType  UniformUsage;
		ShaderStage		  Shaderstage;
	};
	class Pipeline;
	class DescriptorSet
	{
	public:
		DescriptorSet(const std::vector<ShaderBindingSpecs>& shaderBindings);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
		const std::vector<ShaderBindingSpecs>& GetShaderLayout() { return m_ShaderLayout; }
	private:
		VkDescriptorSet					 m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool				 m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout			 m_DescriptorSetLayout = VK_NULL_HANDLE;
		std::vector<ShaderBindingSpecs>  m_ShaderLayout;
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