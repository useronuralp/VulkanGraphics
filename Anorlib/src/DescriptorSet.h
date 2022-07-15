#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
namespace Anor
{
	class LogicalDevice;
	class Pipeline;
	class DescriptorSet
	{
	public:
		DescriptorSet(const Ref<LogicalDevice>& device);
		~DescriptorSet();
		const VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		const VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
	private:
		VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout	m_DescriptorSetLayout = VK_NULL_HANDLE;
		Ref<LogicalDevice> m_Device;
	};
}