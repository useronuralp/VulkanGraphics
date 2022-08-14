#include "Swapchain.h"
#include <algorithm>
#include <iostream>
#include "Surface.h"
#include "RenderPass.h" 
#include "VulkanApplication.h"
#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Window.h" 
namespace Anor
{
	Swapchain::Swapchain(const Ref<RenderPass>& renderPass)
        :m_RenderPass(renderPass)
	{
        Init();
	}
    
    void Swapchain::Init()
    {
        m_DepthBufferFormat = FindDepthFormat();
        // Find a suitable present mode.
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), VulkanApplication::s_Surface->GetVKSurface(), &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes;
        if (presentModeCount != 0)
        {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), VulkanApplication::s_Surface->GetVKSurface(), &presentModeCount, presentModes.data());
        }
        bool found = false;
        for (const auto& availablePresentMode : presentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                found = true;
                m_PresentMode = availablePresentMode;
                break;
            }
        }
        if (!found)
        {
            m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
        }

        // Find the optimal image count to request from the swapchain.
        m_ImageCount = VulkanApplication::s_Surface->GetVKSurfaceCapabilities().minImageCount + 1;

        if (VulkanApplication::s_Surface->GetVKSurfaceCapabilities().maxImageCount > 0 && m_ImageCount > VulkanApplication::s_Surface->GetVKSurfaceCapabilities().maxImageCount)
        {
            m_ImageCount = VulkanApplication::s_Surface->GetVKSurfaceCapabilities().maxImageCount;
        }

        // Create info for the swapchain.
        VkSwapchainCreateInfoKHR CI{};
        CI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        CI.surface = VulkanApplication::s_Surface->GetVKSurface();
        CI.minImageCount = m_ImageCount;
        CI.imageFormat = VulkanApplication::s_Surface->GetVKSurfaceFormat().format;
        CI.imageColorSpace = VulkanApplication::s_Surface->GetVKSurfaceFormat().colorSpace;
        CI.imageExtent = VulkanApplication::s_Surface->GetVKExtent();
        CI.imageArrayLayers = 1;
        CI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        CI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CI.queueFamilyIndexCount = 0;
        CI.pQueueFamilyIndices = nullptr;
        CI.preTransform = VulkanApplication::s_Surface->GetVKSurfaceCapabilities().currentTransform;
        CI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        CI.presentMode = m_PresentMode;
        CI.clipped = VK_TRUE;
        CI.oldSwapchain = VK_NULL_HANDLE;

        ASSERT(vkCreateSwapchainKHR(VulkanApplication::s_Device->GetVKDevice(), &CI, nullptr, &m_Swapchain) == VK_SUCCESS, "Failed to create swap chain!");

        std::cout << "Successfuly created a swapchain!" << std::endl;

        vkGetSwapchainImagesKHR(VulkanApplication::s_Device->GetVKDevice(), m_Swapchain, &m_ImageCount, nullptr);
        m_SwapchainImages.resize(m_ImageCount);
        vkGetSwapchainImagesKHR(VulkanApplication::s_Device->GetVKDevice(), m_Swapchain, &m_ImageCount, m_SwapchainImages.data());

        m_SwapchainImageFormat = VulkanApplication::s_Surface->GetVKSurfaceFormat().format;

        // Create the image views for swapchain images so that we can access them.
        m_ImageViews.resize(m_SwapchainImages.size());
        for (int i = 0; i < m_SwapchainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapchainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_SwapchainImageFormat;

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &createInfo, nullptr, &m_ImageViews[i]) == VK_SUCCESS, "Failed to create image view.");
        }

        // VK Image creation for the depth buffer.
        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.extent.width = VulkanApplication::s_Surface->GetVKExtent().width;
        depthImageInfo.extent.height = VulkanApplication::s_Surface->GetVKExtent().height;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.format = m_DepthBufferFormat;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.flags = 0; // Optional

        ASSERT(vkCreateImage(VulkanApplication::s_Device->GetVKDevice(), &depthImageInfo, nullptr, &m_DepthImage) == VK_SUCCESS, "Failed to create image!");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), m_DepthImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        ASSERT(vkAllocateMemory(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, nullptr, &m_DepthImageMemory) == VK_SUCCESS, "failed to allocate image memory!");
        vkBindImageMemory(VulkanApplication::s_Device->GetVKDevice(), m_DepthImage, m_DepthImageMemory, 0);

        // Depth buffer image view creation.
        VkImageViewCreateInfo depthImageviewInfo{};
        depthImageviewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        depthImageviewInfo.image = m_DepthImage;
        depthImageviewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        depthImageviewInfo.format = m_DepthBufferFormat;
        depthImageviewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        depthImageviewInfo.subresourceRange.baseMipLevel = 0;
        depthImageviewInfo.subresourceRange.levelCount = 1;
        depthImageviewInfo.subresourceRange.baseArrayLayer = 0;
        depthImageviewInfo.subresourceRange.layerCount = 1;

        ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &depthImageviewInfo, nullptr, &m_DepthImageView) == VK_SUCCESS, "Failed to create textre image view");

        // Creating the necessary framebuffers for each of the image view we have in this class.
        if (m_Framebuffers.size() > 0)
        {
            for (int i = 0; i < m_Framebuffers.size(); i++)
            {
                m_Framebuffers[i].reset();
            }
        }
        else
        {
            m_Framebuffers.resize(m_ImageViews.size());
        }

        for (int i = 0; i < m_Framebuffers.size(); i++)
        {
            std::vector<VkImageView> attachments =
            {
                m_ImageViews[i],
                m_DepthImageView
            };
            m_Framebuffers[i] = std::make_shared<Framebuffer>(m_RenderPass, attachments);
        }
    }
    void Swapchain::CleanupSwapchain()
    {
        for (auto imageView : m_ImageViews)
        {
            vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), imageView, nullptr);
        }
        vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), m_DepthImageView, nullptr);
        vkDestroyImage(VulkanApplication::s_Device->GetVKDevice(), m_DepthImage, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_DepthImageMemory, nullptr);
        vkDestroySwapchainKHR(VulkanApplication::s_Device->GetVKDevice(), m_Swapchain, nullptr);
    }
    void Swapchain::OnResize()
    {
        CleanupSwapchain();
        Init();
    }

    VkFormat Swapchain::FindDepthFormat()
    {
        return FindSupportedFormat( { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat Swapchain::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }

        std::cerr << "Failed to find supported format!";
        __debugbreak();
    }

    bool Swapchain::HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    uint32_t Swapchain::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
        }

        std::cerr << "Failed to find suitable memory type" << std::endl;
        __debugbreak();
    }

    Swapchain::~Swapchain()
    {
        CleanupSwapchain();
    }

}