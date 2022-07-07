#pragma once
#include "vulkan/vulkan.h"

namespace Anor
{
	class LogicalDevice;
	class CommandPool
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			uint32_t QueueFamilyIndex; // The index of a queue family that you want the command pool to be linked to.
		};
	public:
		CommandPool(CreateInfo& createInfo);
		~CommandPool();
		VkCommandPool GetCommandPool() { return m_CommandPool; }
	private:
		VkCommandPool m_CommandPool = VK_NULL_HANDLE;
		LogicalDevice* m_Device;
	};
}