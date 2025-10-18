#include "Instance.h"
#include "PhysicalDevice.h"
#include "Surface.h"
// #include "VulkanApplication.h"
#include "Window.h"

#include <algorithm>
Surface::Surface(
    std::shared_ptr<Instance>       InInstance,
    std::shared_ptr<Window>         InWindow,
    std::shared_ptr<PhysicalDevice> InPhysicalDevice)
    : _Instance(InInstance), _Window(InWindow), _PhysicalDevice(InPhysicalDevice)
{
    // 1. Create the Vulkan surface for the given window
    auto result = glfwCreateWindowSurface(_Instance->GetVkInstance(), _Window->GetNativeWindow(), nullptr, &_Surface);

    ASSERT(result == VK_SUCCESS, "Failed to create a window surface");

    // 2. Query capabilities and surface format
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &_Capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    ASSERT(formatCount != 0, "No surface formats found!");

    surfaceFormats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &formatCount, surfaceFormats.data());

    bool found = false;
    for (const auto& availableFormat : surfaceFormats)
    {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            found          = true;
            _SurfaceFormat = availableFormat;
        }
    }

    if (NOT found)
    {
        _SurfaceFormat = surfaceFormats[0];
        found          = true;
    }
}
Surface::Surface()
{
    ASSERT(
        glfwCreateWindowSurface(_Instance->GetVkInstance(), _Window->GetNativeWindow(), nullptr, &_Surface) == VK_SUCCESS,
        "Failed to create a window surface");

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &_Capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &formatCount, nullptr);

    std::vector<VkSurfaceFormatKHR> surfaceFormats;

    if (formatCount != 0)
    {
        surfaceFormats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            _PhysicalDevice->GetVKPhysicalDevice(), _Surface, &formatCount, surfaceFormats.data());
    }

    bool found = false;
    for (const auto& availableFormat : surfaceFormats)
    {
        if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            found          = true;
            _SurfaceFormat = availableFormat;
        }
    }

    if (!found)
    {
        _SurfaceFormat = surfaceFormats[0];
        found          = true;
    }
}

Surface::~Surface()
{
    vkDestroySurfaceKHR(_Instance->GetVkInstance(), _Surface, nullptr);
}

VkExtent2D Surface::GetVKExtent()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_PhysicalDevice->GetVKPhysicalDevice(), _Surface, &_Capabilities);

    if (_Capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
    {
        return _Capabilities.currentExtent;
    }
    else
    {
        int width, height;
        glfwGetFramebufferSize(_Window->GetNativeWindow(), &width, &height);

        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

        actualExtent.width =
            std::clamp(actualExtent.width, _Capabilities.minImageExtent.width, _Capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, _Capabilities.minImageExtent.height, _Capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
