#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Surface.h"
#include "Texture.h"
#include "VulkanApplication.h"
#include <iostream>
namespace OVK
{
	Framebuffer::Framebuffer(const Ref<RenderPass>& renderPass, std::vector<VkImageView> attachments)
	{
        m_Width = VulkanApplication::s_Surface->GetVKExtent().width;
        m_Height = VulkanApplication::s_Surface->GetVKExtent().height;
        
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
    Framebuffer::Framebuffer(const Ref<RenderPass>& renderPass, const Ref<Texture>& texture)
    {
        m_Width = texture->GetWidth();
        m_Height = texture->GetHeight();

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass->GetRenderPass();
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &texture->GetImage()->GetImageView();
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