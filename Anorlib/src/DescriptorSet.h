#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <vector>
namespace Anor
{
	enum class ShaderBinding
	{
		IMAGE_SAMPLER,
		UNIFORM_BUFFER,
	};

	class Pipeline;
	class DescriptorSet
	{
	public:
		DescriptorSet(const std::vector<ShaderBinding>& shaderBindings);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
	private:
		VkDescriptorSet  m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout	m_DescriptorSetLayout = VK_NULL_HANDLE;
	};
}