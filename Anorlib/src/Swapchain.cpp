#include "Swapchain.h"
#include <algorithm>
#include <iostream>
#include "Surface.h"
#include "RenderPass.h" 
#include "Framebuffer.h"
namespace Anor
{
	Swapchain::Swapchain(CreateInfo& createInfo)
        :m_PhysicalDevice(createInfo.pPhysicalDevice), m_Surface(createInfo.pSurface), m_LogicalDevice(createInfo.pLogicalDevice), m_RenderPass(createInfo.pRenderPass)
	{
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice->GetVKPhysicalDevice(), m_Surface->GetVKSurface(), &presentModeCount, nullptr);

        std::vector<VkPresentModeKHR> presentModes;

        if (presentModeCount != 0)
        {
            presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice->GetVKPhysicalDevice(), m_Surface->GetVKSurface(), &presentModeCount, presentModes.data());
        }

        VkPresentModeKHR presentMode;
        bool found = false;

        for (const auto& availablePresentMode : presentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                found = true;
                presentMode = availablePresentMode;
            }
        }
        
        if (!found)
        {
            presentMode = VK_PRESENT_MODE_FIFO_KHR; 
        }

        uint32_t imageCount = m_Surface->GetVKSurfaceCapabilities().minImageCount + 1;

        if (m_Surface->GetVKSurfaceCapabilities().maxImageCount > 0 && imageCount > m_Surface->GetVKSurfaceCapabilities().maxImageCount)
        {
            imageCount = m_Surface->GetVKSurfaceCapabilities().maxImageCount;
        }

        VkSwapchainCreateInfoKHR CI{};

        CI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        CI.surface = m_Surface->GetVKSurface();
        CI.minImageCount = imageCount;
        CI.imageFormat = m_Surface->GetVKSurfaceFormat().format;
        CI.imageColorSpace = m_Surface->GetVKSurfaceFormat().colorSpace;
        CI.imageExtent = m_Surface->GetVKExtent();
        CI.imageArrayLayers = 1; 
        CI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        CI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        CI.queueFamilyIndexCount = 0; 
        CI.pQueueFamilyIndices = nullptr;
        CI.preTransform = m_Surface->GetVKSurfaceCapabilities().currentTransform;
        CI.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        CI.presentMode = m_PresentMode;
        CI.clipped = VK_TRUE; 
        CI.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_LogicalDevice->GetVKDevice(), &CI, nullptr, &m_Swapchain) != VK_SUCCESS)
        {
            std::cerr << "Failed to create swap chain!" << std::endl;
            __debugbreak();
        }
        else
        {
            std::cout << "Successfuly created a swapchain!" << std::endl;
        }

        vkGetSwapchainImagesKHR(m_LogicalDevice->GetVKDevice(), m_Swapchain, &imageCount, nullptr);
        m_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_LogicalDevice->GetVKDevice(), m_Swapchain, &imageCount, m_SwapchainImages.data());

        // TO DO: Might not be a good idead to store these here in member variables again. We have this information already inside the m_Surface variable.
        m_SwapchainImageFormat = m_Surface->GetVKSurfaceFormat().format;
        m_SwapchainExtent = m_Surface->GetVKExtent();


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

            if (vkCreateImageView(m_LogicalDevice->GetVKDevice(), &createInfo, nullptr, &m_ImageViews[i]) != VK_SUCCESS)
            {
                std::cerr << "Failed to create image views." << std::endl;
                __debugbreak();
            }
        }

        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        if (vkCreateSemaphore(m_LogicalDevice->GetVKDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateFence(m_LogicalDevice->GetVKDevice(), &fenceCreateInfo, nullptr, &m_InRenderingFence) != VK_SUCCESS ||
            vkCreateSemaphore(m_LogicalDevice->GetVKDevice(), &semaphoreInfo, nullptr, &m_RenderingCompleteSemaphore) != VK_SUCCESS)
        {
            std::cerr << "Failed to create semaphores" << std::endl;
            __debugbreak();
        }

        // Createing the necessary framebuffers for each of the image view we have in this class.
        m_Framebuffers.resize(m_ImageViews.size());
        for (int i = 0; i < m_ImageViews.size(); i++)
        {
            Framebuffer::CreateInfo CI{};
            CI.pLogicalDevice = m_LogicalDevice;
            CI.pRenderPass = m_RenderPass;
            CI.pAttachments = &m_ImageViews[i];
            CI.ExtentHeight = m_SwapchainExtent.height;
            CI.ExtentWidth = m_SwapchainExtent.width;
            m_Framebuffers[i] = new Framebuffer(CI);
        }
	}

    uint32_t Swapchain::AcquireNextImage()
    {
        uint32_t imageIndex = -1;
        VkResult result = vkAcquireNextImageKHR(m_LogicalDevice->GetVKDevice(), m_Swapchain, UINT64_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            std::cerr << "Out of date!" << std::endl;
            __debugbreak();
            //RecreateSwapChain();
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            std::cerr << "Failed to acquire swap chain image" << std::endl;
            __debugbreak();
        }
        return imageIndex;
    }

    void Swapchain::ResetFence()
    {
        vkResetFences(m_LogicalDevice->GetVKDevice(), 1, &m_InRenderingFence);
    }

    VkResult Swapchain::QueuePresent(const VkQueue& presentQueue, const VkSemaphore& waitSemaphore)
    {
        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &waitSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_ActiveImageIndex;
        return vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    Swapchain::~Swapchain()
    {
        for (auto imageView : m_ImageViews)
        {
            vkDestroyImageView(m_LogicalDevice->GetVKDevice(), imageView, nullptr);
        }
        vkDestroySwapchainKHR(m_LogicalDevice->GetVKDevice(), m_Swapchain, nullptr);
        vkDestroySemaphore(m_LogicalDevice->GetVKDevice(), m_ImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(m_LogicalDevice->GetVKDevice(), m_RenderingCompleteSemaphore, nullptr);
        vkDestroyFence(m_LogicalDevice->GetVKDevice(), m_InRenderingFence, nullptr);

        for (auto FB : m_Framebuffers)
        {
            delete FB;
        }
    }

}