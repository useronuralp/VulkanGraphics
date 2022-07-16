#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Surface.h"
#include <iostream>
namespace Anor
{
	Framebuffer::Framebuffer(const Ref<LogicalDevice>& device, const Ref<RenderPass>& renderPass, const Ref<Surface>& surface, std::vector<VkImageView> attachments)
        :m_Device(device), m_RenderPass(renderPass)
	{
            m_Width = surface->GetVKExtent().width;
            m_Height = surface->GetVKExtent().height;

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_Width;
            framebufferInfo.height = m_Height;
            framebufferInfo.layers = 1;

            ASSERT(vkCreateFramebuffer(m_Device->GetVKDevice(), &framebufferInfo, nullptr, &m_Framebuffer) == VK_SUCCESS, "Failed to create framebuffer!");
	}
    Framebuffer::~Framebuffer()
    {
        vkDestroyFramebuffer(m_Device->GetVKDevice(), m_Framebuffer, nullptr);
    }
}