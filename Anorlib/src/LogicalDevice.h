#pragma once
#include "vulkan/vulkan.h"
#include <vector>
namespace Anor
{
	class Window;
	class Surface;
	class LogicalDevice
	{
		friend class PhysicalDevice;
	public:
		struct QueueCreateInfo
		{
			uint32_t							  QueueCount;
			uint32_t							  QueueFamilyIndex = -1;
			const float*						  pQueuePriorities;
			bool								  SearchForPresentSupport;
			VkDeviceQueueCreateFlags			  Flags;
			VkQueueFlagBits						  QueueType;
		};

		struct CreateInfo
		{
			std::vector<QueueCreateInfo>		  ppQueueCreateInfos;
			std::vector<const char*>			  ppEnabledExtensionNames;
			std::vector<VkPhysicalDeviceFeatures> pEnabledFeatures;
			std::vector<const char*>			  pEnabledLayerNames;
			VkDeviceCreateFlags					  Flags;
			// Vulkan Objects-----------------------------------------------------
			PhysicalDevice*						  pPhysicalDevice;
			Surface*							  pSurface;
			Window*								  pWindow;
		};

		LogicalDevice(CreateInfo& createInfo, std::vector<QueueCreateInfo>& queueCreateInfos);
		~LogicalDevice();
	public:
		VkDevice& GetVKDevice() { return m_Device; }
		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
	private:
		VkDevice				 m_Device		 = VK_NULL_HANDLE;
		VkQueue					 m_GraphicsQueue = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		PhysicalDevice*			 m_PhysicalDevice;
		Surface*				 m_Surface;
		Window*					 m_Window;

		std::vector<const char*> m_Layers;
		std::vector<const char*> m_Extensions;

		// TO DO: Create a logic to set the following queues later.
		//VkQueue					 m_PresentQueue = VK_NULL_HANDLE;
		//VkQueue					 m_TransferQueue = VK_NULL_HANDLE;
		//VkQueue					 m_ComputeQueue = VK_NULL_HANDLE;

	};
}