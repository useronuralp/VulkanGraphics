#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace Anor
{
	class Surface
	{
	public:
		Surface();
		~Surface();
	public:
		VkExtent2D						 GetVKExtent();
		const VkSurfaceKHR&				 GetVKSurface()				{ return m_Surface;		  }
		const VkSurfaceFormatKHR&		 GetVKSurfaceFormat()		{ return m_SurfaceFormat; }
		const VkSurfaceCapabilitiesKHR&  GetVKSurfaceCapabilities() { return m_Capabilities;  }
	private:
		VkSurfaceKHR			 m_Surface = VK_NULL_HANDLE;
		VkSurfaceFormatKHR		 m_SurfaceFormat;
		VkSurfaceCapabilitiesKHR m_Capabilities;
	};
}