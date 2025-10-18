#include "Engine.h"
#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "Surface.h"
#include "VulkanContext.h"

#include <iostream>
Framebuffer::Framebuffer(
    const VkRenderPass&            InRenderPass,
    const std::vector<VkImageView> InAttachments,
    uint32_t                       InWidth,
    uint32_t                       InHeight,
    int                            InLayerCount)
    : _Width(InWidth), _Height(InHeight)
{
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass      = InRenderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(InAttachments.size());
    framebufferInfo.pAttachments    = InAttachments.data();
    framebufferInfo.width           = _Width;
    framebufferInfo.height          = _Height;
    framebufferInfo.layers          = InLayerCount;

    ASSERT(
        vkCreateFramebuffer(Engine::GetContext().GetDevice()->GetVKDevice(), &framebufferInfo, nullptr, &_Framebuffer) ==
            VK_SUCCESS,
        "Failed to create framebuffer!");
}

const uint32_t& Framebuffer::GetWidth()
{
    return _Width;
}
const uint32_t& Framebuffer::GetHeight()
{
    return _Height;
}
const VkFramebuffer& Framebuffer::GetHandle()
{
    return _Framebuffer;
}

Framebuffer::~Framebuffer()
{
    vkDestroyFramebuffer(Engine::GetContext().GetDevice()->GetVKDevice(), _Framebuffer, nullptr);
}
