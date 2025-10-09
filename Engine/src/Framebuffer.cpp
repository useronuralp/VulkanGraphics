#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "Surface.h"
#include "VulkanApplication.h"
#include <iostream>
namespace OVK
{
	Framebuffer::Framebuffer(const VkRenderPass& renderPass, const std::vector<VkImageView> attachments, uint32_t width, uint32_t height, int layerCount)
	{
        m_Width = width;
        m_Height = height;
        
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = layerCount;
        
        ASSERT(vkCreateFramebuffer(VulkanApplication::s_Device->GetVKDevice(), &framebufferInfo, nullptr, &m_Framebuffer) == VK_SUCCESS, "Failed to create framebuffer!");
	}

    Framebuffer::~Framebuffer()
    {
        vkDestroyFramebuffer(VulkanApplication::s_Device->GetVKDevice(), m_Framebuffer, nullptr);
    }
}