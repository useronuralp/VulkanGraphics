#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Surface.h"
#include "Swapchain.h"
#include "VulkanContext.h"

#include <iostream>

Swapchain::Swapchain(VulkanContext& InContext) : _Context(InContext)
{
    Create();
}

const VkSwapchainKHR Swapchain::GetHandle() const noexcept
{
    return _Swapchain;
}
const VkFormat Swapchain::GetImageFormat() const noexcept
{
    return _Format;
}
const VkPresentModeKHR Swapchain::GetPresentMode() const noexcept
{
    return _PresentMode;
}
const std::vector<VkImage>& Swapchain::GetImages() const noexcept
{
    return _Images;
}
const std::vector<VkImageView>& Swapchain::GetImageViews() const noexcept
{
    return _ImageViews;
}

void Swapchain::Create()
{
    // Query present modes
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        _Context.GetPhysicalDevice()->GetVKPhysicalDevice(), _Context.GetSurface()->GetVKSurface(), &presentModeCount, nullptr);

    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0)
    {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            _Context.GetPhysicalDevice()->GetVKPhysicalDevice(),
            _Context.GetSurface()->GetVKSurface(),
            &presentModeCount,
            presentModes.data());
    }

    _PresentMode = VK_PRESENT_MODE_FIFO_KHR; // fallback
    for (const auto& mode : presentModes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            _PresentMode = mode;
            break;
        }
    }

    // Determine image count
    _ImageCount         = _Context.GetSurface()->GetVKSurfaceCapabilities().minImageCount + 1;
    const auto maxCount = _Context.GetSurface()->GetVKSurfaceCapabilities().maxImageCount;
    if (maxCount > 0 && _ImageCount > maxCount)
        _ImageCount = maxCount;

    // Create swapchain
    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = _Context.GetSurface()->GetVKSurface();
    ci.minImageCount    = _ImageCount;
    ci.imageFormat      = _Context.GetSurface()->GetVKSurfaceFormat().format;
    ci.imageColorSpace  = _Context.GetSurface()->GetVKSurfaceFormat().colorSpace;
    ci.imageExtent      = _Context.GetSurface()->GetVKExtent();
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform     = _Context.GetSurface()->GetVKSurfaceCapabilities().currentTransform;
    ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode      = _PresentMode;
    ci.clipped          = VK_TRUE;
    ci.oldSwapchain     = VK_NULL_HANDLE;

    ASSERT(
        vkCreateSwapchainKHR(_Context.GetDevice()->GetVKDevice(), &ci, nullptr, &_Swapchain) == VK_SUCCESS,
        "Failed to create swap chain!");
    PrintInfo("Successfully created swapchain!");

    // Retrieve images
    vkGetSwapchainImagesKHR(_Context.GetDevice()->GetVKDevice(), _Swapchain, &_ImageCount, nullptr);
    _Images.resize(_ImageCount);
    vkGetSwapchainImagesKHR(_Context.GetDevice()->GetVKDevice(), _Swapchain, &_ImageCount, _Images.data());

    _Format = _Context.GetSurface()->GetVKSurfaceFormat().format;

    // Create image views
    _ImageViews.resize(_Images.size());
    for (size_t i = 0; i < _Images.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = _Images[i];
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = _Format;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        ASSERT(
            vkCreateImageView(_Context.GetDevice()->GetVKDevice(), &viewInfo, nullptr, &_ImageViews[i]) == VK_SUCCESS,
            "Failed to create image view.");
    }
}
void Swapchain::Cleanup()
{
    for (const auto& imageView : _ImageViews)
    {
        vkDestroyImageView(_Context.GetDevice()->GetVKDevice(), imageView, nullptr);
    }

    if (_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(_Context.GetDevice()->GetVKDevice(), _Swapchain, nullptr);
    }
}
void Swapchain::Recreate()
{
    Cleanup();
    Create();
}

Swapchain::~Swapchain() noexcept
{
    Cleanup();
}
