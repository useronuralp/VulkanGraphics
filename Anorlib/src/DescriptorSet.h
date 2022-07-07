#pragma once
#include "vulkan/vulkan.h"
namespace Anor
{
	class LogicalDevice;
	class Pipeline;
	class DescriptorSet
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
		};
	public:
		DescriptorSet(CreateInfo& createInfo);
		~DescriptorSet();
		VkDescriptorSet& GetVKDescriptorSet() { return m_DescriptorSet; }
		VkDescriptorSetLayout& GetVKDescriptorSetLayout() { return m_DescriptorSetLayout; }
	private:
		VkDescriptorSet m_DescriptorSet = VK_NULL_HANDLE;
		VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
		VkDescriptorSetLayout	m_DescriptorSetLayout = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		LogicalDevice* m_Device;
	};
}