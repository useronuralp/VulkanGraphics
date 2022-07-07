#pragma once
#include "vulkan/vulkan.h"
#include <vector>
namespace Anor
{
	class Instance;
	class Window;
	class PhysicalDevice;
	class Surface
	{
	public:
		struct CreateInfo
		{
			Instance* pInstance;
			PhysicalDevice* pPhysicalDevice;
			Window* pWindow;
		};
	public:
		Surface(CreateInfo& createInfo);
		~Surface();
		VkSurfaceKHR			 GetVKSurface()			  { return m_Surface;		}
		VkExtent2D				 GetVKExtent()			  { return m_Extent;		}
		VkSurfaceFormatKHR		 GetVKSurfaceFormat()	  { return m_SurfaceFormat; }
		VkSurfaceCapabilitiesKHR GetVKSurfaceCapabilities() { return m_Capabilities;  }
	private:
		VkSurfaceKHR			 m_Surface = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTER SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		Instance*				 m_Instance;
		VkSurfaceCapabilitiesKHR m_Capabilities;
		VkSurfaceFormatKHR		 m_SurfaceFormat;
		VkExtent2D				 m_Extent;
	};
}