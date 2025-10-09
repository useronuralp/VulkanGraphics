#include "Surface.h"
#include "Instance.h"
#include "Window.h"
#include "PhysicalDevice.h"
#include "VulkanApplication.h"
#include <algorithm>
namespace OVK
{
	Surface::Surface()
	{
		ASSERT(glfwCreateWindowSurface(VulkanApplication::s_Instance->GetVkInstance(), VulkanApplication::s_Window->GetNativeWindow(), nullptr, &m_Surface) == VK_SUCCESS, "Failed to create a window surface");

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), m_Surface, &m_Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), m_Surface, &formatCount, nullptr);

		std::vector<VkSurfaceFormatKHR> surfaceFormats;

		if (formatCount != 0)
		{
			surfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), m_Surface, &formatCount, surfaceFormats.data());
		}

		bool found = false;
		for (const auto& availableFormat : surfaceFormats)
		{
			if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				found = true;
				m_SurfaceFormat = availableFormat;
			}
		}

		if (!found)
		{
			m_SurfaceFormat = surfaceFormats[0];
			found = true;
		}
	}

	Surface::~Surface()
	{
		vkDestroySurfaceKHR(VulkanApplication::s_Instance->GetVkInstance(), m_Surface, nullptr);
	}

	VkExtent2D Surface::GetVKExtent()
	{
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), m_Surface, &m_Capabilities);

		if (m_Capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
		{
			return m_Capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(VulkanApplication::s_Window->GetNativeWindow(), &width, &height);

			VkExtent2D actualExtent =
			{
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, m_Capabilities.minImageExtent.width, m_Capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, m_Capabilities.minImageExtent.height, m_Capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

}