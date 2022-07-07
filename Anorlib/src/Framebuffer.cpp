#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include <iostream>
namespace Anor
{
    // TO DO: Convert the third parameter into a custom class called "Attachment".
	Framebuffer::Framebuffer(CreateInfo& createInfo)
        :m_Device(createInfo.pLogicalDevice), m_RenderPass(createInfo.pRenderPass)
	{
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass->GetRenderPass();
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = createInfo.pAttachments;
            framebufferInfo.width = createInfo.ExtentWidth;
            framebufferInfo.height = createInfo.ExtentHeight;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Device->GetVKDevice(), &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS)
            {
                std::cerr << "Failed to create framebuffer!" << std::endl;
                __debugbreak();
            }
	}
    Framebuffer::~Framebuffer()
    {
        vkDestroyFramebuffer(m_Device->GetVKDevice(), m_Framebuffer, nullptr);
    }
}