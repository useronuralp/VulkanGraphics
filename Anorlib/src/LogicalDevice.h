#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace Anor
{
	class Window;
	class Surface;
	class PhysicalDevice;
	class LogicalDevice
	{
	public:
		LogicalDevice(const Ref<PhysicalDevice>& physDevice, const Ref<Surface>& surface, const Ref<Window>& window, uint32_t graphicsQueueIndex, uint32_t presentQueueIndex);
		~LogicalDevice();
	public:
		const VkDevice& GetVKDevice() { return m_Device; }
		const VkQueue&  GetGraphicsQueue() { return m_GraphicsQueue; }
		const VkQueue&  GetPresentQueue() { return m_PresentQueue; }
	private:
		VkDevice				 m_Device		 = VK_NULL_HANDLE;
		VkQueue					 m_GraphicsQueue = VK_NULL_HANDLE;
		VkQueue					 m_PresentQueue  = VK_NULL_HANDLE;

		Ref<PhysicalDevice>		 m_PhysicalDevice;
		Ref<Surface>			 m_Surface;
		Ref<Window>				 m_Window;

		std::vector<const char*> m_Layers;
		std::vector<const char*> m_Extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

		// TO DO: Create a logic to set the following queues later.
		//VkQueue					 m_TransferQueue = VK_NULL_HANDLE;
		//VkQueue					 m_ComputeQueue = VK_NULL_HANDLE;

	};
}