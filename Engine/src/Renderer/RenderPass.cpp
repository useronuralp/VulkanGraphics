#include "core.h"
#include "Framebuffer.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Surface.h"
#include "VulkanContext.h"

#include <vulkan/vulkan.h>
void RenderPass::CreateRenderPass()
{
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference>   colorRefs;
    VkAttachmentReference                depthRef{};

    uint32_t index = 0;
    for (auto& attachment : _Info.Attachments) {
        VkAttachmentDescription desc{};
        desc.format         = attachment.Format;
        desc.samples        = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp         = attachment.LoadOp;
        desc.storeOp        = attachment.StoreOp;
        desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout    = attachment.FinalLayout;

        attachments.push_back(desc);

        if (attachment.FinalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
            attachment.FinalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
            depthRef = { index, attachment.FinalLayout };
        else
            colorRefs.push_back({ index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        ++index;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = (uint32_t)colorRefs.size();
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = _Info.HasDepth ? &depthRef : nullptr;

    VkSubpassDependency defaultDependency{};
    bool                useDefaultDep = (uint32_t)_Info.Dependencies.empty();
    // TODO: Add ensure macro here
    if (useDefaultDep) {
        VkSubpassDependency defaultDependency{};
        defaultDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        defaultDependency.dstSubpass    = 0;
        defaultDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.srcAccessMask = 0;
        defaultDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (uint32_t)attachments.size();
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    if (useDefaultDep) {
        rpInfo.dependencyCount = 1;
        rpInfo.pDependencies   = &defaultDependency;
    } else {
        rpInfo.dependencyCount = static_cast<uint32_t>(_Info.Dependencies.size());
        rpInfo.pDependencies   = _Info.Dependencies.data();
    }

    ASSERT(vkCreateRenderPass(_Context.GetDevice()->GetVKDevice(), &rpInfo, nullptr, &_RenderPass) == VK_SUCCESS, "Failed");
}
void RenderPass::Destroy()
{
    VkDevice device = _Context.GetDevice()->GetVKDevice();
    if (_RenderPass)
        vkDestroyRenderPass(device, _RenderPass, nullptr);
}

RenderPass::RenderPass(VulkanContext& InContext, const CreateInfo& InInfo) : _Context(InContext), _Info(InInfo)
{
    CreateRenderPass();
}

RenderPass::~RenderPass()
{
    Destroy();
}

void RenderPass::Begin(VkCommandBuffer InCmdBuffer, Framebuffer& InFramebuffer)
{
    std::vector<VkClearValue> clearValues;
    for (auto& attachment : _Info.Attachments)
        clearValues.push_back(attachment.ClearValue);

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass               = _RenderPass;
    beginInfo.framebuffer              = InFramebuffer.GetVKFramebuffer();
    beginInfo.renderArea.extent.height = InFramebuffer.GetHeight();
    beginInfo.renderArea.extent.width  = InFramebuffer.GetWidth();
    beginInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues             = clearValues.data();
    beginInfo.pNext                    = nullptr;

    vkCmdBeginRenderPass(InCmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void RenderPass::End(VkCommandBuffer InCmdBuffer)
{
    vkCmdEndRenderPass(InCmdBuffer);
}

VkRenderPass RenderPass::GetHandle() const
{
    return _RenderPass;
}