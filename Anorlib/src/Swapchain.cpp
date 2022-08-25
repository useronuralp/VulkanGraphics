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
	Swapchain::Swapchain()
	{
        // Creating a custom render pass for drawing the models starts here.
        std::vector<VkFormat> candidates =
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        // Render pass creation. Entire Vulkan API is exposed here because we want this part to be customizable.

        VkAttachmentDescription colorAttachmentDescription;
        VkAttachmentReference colorAttachmentRef;
        VkAttachmentDescription depthAttachmentDescription;
        VkAttachmentReference depthAttachmentRef;

        colorAttachmentDescription.format = VulkanApplication::s_Surface->GetVKSurfaceFormat().format;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.flags = 0;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        depthAttachmentDescription.format = FindSupportedFormat(candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.flags = 0;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachmentDescriptions;
        attachmentDescriptions[0] = colorAttachmentDescription;
        attachmentDescriptions[1] = depthAttachmentDescription;


        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()); // Number of attachments.
        renderPassInfo.pAttachments = attachmentDescriptions.data(); // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VkRenderPass swapchainRenderPass;

        ASSERT(vkCreateRenderPass(VulkanApplication::s_Device->GetVKDevice(), &renderPassInfo, nullptr, &swapchainRenderPass) == VK_SUCCESS, "Failed to create a render pass.");
        m_RenderPass = std::make_shared<RenderPass>(swapchainRenderPass);

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
        m_PresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
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