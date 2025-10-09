#include "Swapchain.h"
#include <algorithm>
#include <iostream>
#include "Surface.h"
#include "VulkanApplication.h"
#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Window.h" 
#include "Utils.h"
namespace OVK
{
	Swapchain::Swapchain()
	{
        // Create the render pass for the swapchain.
        VkAttachmentDescription colorAttachmentDescription;
        VkAttachmentReference colorAttachmentRef;

        // A single color attachment.
        colorAttachmentDescription.format = VulkanApplication::s_Surface->GetVKSurfaceFormat().format;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.flags = 0;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Color att. reference.
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        //subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        // TO DO: The following layout transition may be incorrect check.
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT ;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::array<VkAttachmentDescription, 1> attachmentDescriptions;
        attachmentDescriptions[0] = colorAttachmentDescription;
        //attachmentDescriptions[1] = depthAttachmentDescription;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()); // Number of attachments.
        renderPassInfo.pAttachments = attachmentDescriptions.data(); // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;


        ASSERT(vkCreateRenderPass(VulkanApplication::s_Device->GetVKDevice(), &renderPassInfo, nullptr, &m_RenderPass) == VK_SUCCESS, "Failed to create a render pass.");
        Init();
	}
    
    void Swapchain::Init()
    {
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
        //m_PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        // Find the optimal image count to request from the swapchain.
        m_ImageCount = VulkanApplication::s_Surface->GetVKSurfaceCapabilities().minImageCount + 1;

        // Check if we are trying to request more images than the maximum supported value. If yes clamp it.
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

        // If we already have the framebuffers created reset them.
        if (m_Framebuffers.size() > 0)
        {
            for (int i = 0; i < m_Framebuffers.size(); i++)
            {
                m_Framebuffers[i].reset();
            }
        }
        // If this is the first time we are initializing the framebuffers resize the vector.
        else
        {
            m_Framebuffers.resize(m_ImageViews.size());
        }

        // Fill the frambuffer vector.
        for (int i = 0; i < m_Framebuffers.size(); i++)
        {
            // Creating a framebuffer for each swapchain image. The depth image is shared across the images.
            std::vector<VkImageView> attachments =
            {
                m_ImageViews[i],
            };
            m_Framebuffers[i] = std::make_shared<Framebuffer>(m_RenderPass, attachments, VulkanApplication::s_Surface->GetVKExtent().width, VulkanApplication::s_Surface->GetVKExtent().height);
        }
    }
    void Swapchain::CleanupSwapchain()
    {
        // We need this helper function here to help us cleanup the swapchain because this is potentially a very common task (Eg: window resize).
        for (auto imageView : m_ImageViews)
        {
            vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), imageView, nullptr);
        }
        vkDestroySwapchainKHR(VulkanApplication::s_Device->GetVKDevice(), m_Swapchain, nullptr);
    }
    void Swapchain::OnResize()
    {
        CleanupSwapchain();
        Init();
    }

    const VkFramebuffer& Swapchain::GetActiveFramebuffer()
    {
        return m_Framebuffers[VulkanApplication::GetActiveImageIndex()]->GetVKFramebuffer();
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
        ASSERT(false, "Failed to find depth format");
    }

    bool Swapchain::HasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }


    Swapchain::~Swapchain()
    {
        CleanupSwapchain();
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), m_RenderPass, nullptr);
    }

}