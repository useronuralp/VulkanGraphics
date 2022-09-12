#include "RenderPass.h"
#include "VulkanApplication.h"
#include "Surface.h"
#include "LogicalDevice.h"
#include <iostream>
#include <array>
namespace Anor
{
	RenderPass::RenderPass(std::vector<Attachment>& colorAttachments, std::vector<Attachment>& depthAttachments)
	{
        std::vector<VkAttachmentReference>   clrAttRefs;
        std::vector<VkAttachmentReference>   depthAttRefs;
        std::vector<VkAttachmentDescription> attachmentDescs;
        for (auto& attachment : colorAttachments)
        {
            clrAttRefs.push_back(attachment.GetAttachmentReference());
            attachmentDescs.push_back(attachment.GetAttachmentDescription());
        }
        for (auto& attachment : depthAttachments)
        {
            depthAttRefs.push_back(attachment.GetAttachmentReference());
            attachmentDescs.push_back(attachment.GetAttachmentDescription());
        }

        // Vulkan spilts a render pass into one or more subpasses. Each render pass has to has at least one subpass.
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount    = static_cast<uint32_t>(clrAttRefs.size());
        subpass.pColorAttachments       = clrAttRefs.empty() ? nullptr : clrAttRefs.data();
        subpass.pDepthStencilAttachment = depthAttRefs.empty() ? nullptr : depthAttRefs.data();

        VkSubpassDependency dependency{};
        dependency.srcSubpass       = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass       = 0;
        dependency.srcStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask    = 0;
        dependency.dstStageMask     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask    = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;


        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType            = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount  = static_cast<uint32_t>(attachmentDescs.size()); // Number of attachments.
        renderPassInfo.pAttachments     = attachmentDescs.data(); // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount     = 1;
        renderPassInfo.pSubpasses       = &subpass;
        renderPassInfo.dependencyCount  = 1;
        renderPassInfo.pDependencies    = &dependency;

        ASSERT(vkCreateRenderPass(VulkanApplication::s_Device->GetVKDevice(), &renderPassInfo, nullptr, &m_RenderPass) == VK_SUCCESS, "Failed to create a render pass.");
	}
    RenderPass::~RenderPass()
    {
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), m_RenderPass, nullptr);
    }
    Attachment::Attachment(CreateInfo CI)
    {
        // Fill up descriptor.
        m_Desc.format         = CI.Format; 
        m_Desc.samples        = CI.SampleCount;
        m_Desc.loadOp         = CI.LoadOp;
        m_Desc.storeOp        = CI.StoreOp;
        m_Desc.stencilLoadOp  = CI.StencilLoadOp;
        m_Desc.stencilStoreOp = CI.StencilStoreOp;
        m_Desc.initialLayout  = CI.InitialLayout;
        m_Desc.finalLayout    = CI.FinalLayout;
        m_Desc.flags          = 0;

        // Fill up reference.
        m_Ref.attachment      = CI.Index; // Order of the attachment, color attachment is our first attachemnt so it gets the index of 0. The next one will get index of 1.
        m_Ref.layout          = FromAnorLayoutTypeToVulkan(CI.Type);
    }

}