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
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = surface->GetVKExtent().width;
            framebufferInfo.height = surface->GetVKExtent().height;
            framebufferInfo.layers = 1;

            ASSERT(vkCreateFramebuffer(m_Device->GetVKDevice(), &framebufferInfo, nullptr, &m_Framebuffer) == VK_SUCCESS, "Failed to create framebuffer!");
	}
    Framebuffer::~Framebuffer()
    {
        vkDestroyFramebuffer(m_Device->GetVKDevice(), m_Framebuffer, nullptr);
    }
}