#include "Surface.h"
#include "Instance.h"
#include "Window.h"
#include "PhysicalDevice.h"
#include <algorithm>
namespace Anor
{
	Surface::Surface(CreateInfo& createInfo)
		:m_Instance(createInfo.pInstance)
	{
		if (glfwCreateWindowSurface(m_Instance->GetVkInstance(), createInfo.pWindow->GetNativeWindow(), nullptr, &m_Surface) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a window surface");
		}

		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(createInfo.pPhysicalDevice->GetVKPhysicalDevice(), m_Surface, &m_Capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(createInfo.pPhysicalDevice->GetVKPhysicalDevice(), m_Surface, &formatCount, nullptr);

		std::vector<VkSurfaceFormatKHR> surfaceFormats;

		if (formatCount != 0)
		{
			surfaceFormats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(createInfo.pPhysicalDevice->GetVKPhysicalDevice(), m_Surface, &formatCount, surfaceFormats.data());
		}

		bool found = false;
		for (const auto& availableFormat : surfaceFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				found = true;
				m_SurfaceFormat = availableFormat;
			}
		}

		if (!found)
		{
			m_SurfaceFormat = surfaceFormats[0];
		}
		found = false;


		if (m_Capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) // TO DO: This part could be problematic. Two macros clash for some reason.
		{
			m_Extent = m_Capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(createInfo.pWindow->GetNativeWindow(), &width, &height);

			VkExtent2D actualExtent =
			{
				static_cast<uint32_t>(width),
				static_cast<uint32_t>(height)
			};

			actualExtent.width = std::clamp(actualExtent.width, m_Capabilities.minImageExtent.width, m_Capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, m_Capabilities.minImageExtent.height, m_Capabilities.maxImageExtent.height);

			m_Extent = actualExtent;
		}
	}

	Surface::~Surface()
	{
		vkDestroySurfaceKHR(m_Instance->GetVkInstance(), m_Surface, nullptr);
	}

}