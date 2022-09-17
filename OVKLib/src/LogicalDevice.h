#pragma once
#include "core.h"
// External
#include <vulkan/vulkan.h>
#include <vector>
namespace OVK
{
	class LogicalDevice
	{
	public:
		LogicalDevice(std::vector<const char*> extensions);
		~LogicalDevice();
	public:
		const VkDevice& GetVKDevice()		{ return m_Device; }
		const VkQueue&  GetGraphicsQueue()	{ return m_GraphicsQueue; }
		const VkQueue&  GetPresentQueue()	{ return m_PresentQueue; }
	private:
		VkQueueFamilyProperties GetQueueFamilyProps(uint64_t queueFamilyIndex);
	private:
		VkDevice m_Device		 = VK_NULL_HANDLE;
		VkQueue	 m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue	 m_PresentQueue  = VK_NULL_HANDLE;
		VkQueue  m_ComputeQueue  = VK_NULL_HANDLE;

		std::vector<const char*> m_Layers;
		std::vector<const char*> m_DeviceExtensions;
	};
}