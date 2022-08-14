#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Surface.h"
#include "VulkanApplication.h"
#include <iostream>
namespace Anor
{
	Framebuffer::Framebuffer(const Ref<RenderPass>& renderPass, std::vector<VkImageView> attachments, uint32_t width, uint32_t height)
	{
        if (width == UINT32_MAX && height == UINT32_MAX)
        {
            m_Width = VulkanApplication::s_Surface->GetVKExtent().width;
            m_Height = VulkanApplication::s_Surface->GetVKExtent().height;
        }
        else
        {
            m_Width = width;
            m_Height = height;
        }
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->GetRenderPass();
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;
        
        ASSERT(vkCreateFramebuffer(VulkanApplication::s_Device->GetVKDevice(), &framebufferInfo, nullptr, &m_Framebuffer) == VK_SUCCESS, "Failed to create framebuffer!");
	}
    Framebuffer::~Framebuffer()
    {
        vkDestroyFramebuffer(VulkanApplication::s_Device->GetVKDevice(), m_Framebuffer, nullptr);
    }
}