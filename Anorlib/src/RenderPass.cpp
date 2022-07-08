#include "RenderPass.h"
#include "LogicalDevice.h"
#include <iostream>
namespace Anor
{
	RenderPass::RenderPass(CreateInfo& createInfo)
		:m_Device(createInfo.pLogicalDevice)
	{
        // Color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = createInfo.Format; // Format of the color attachment should match the swap chain image format.
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Colot attachment ref.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // Order of the attachment, color attachment is our first attachemnt so it gets the index of 0. The next one will get index of 1.
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Vulkan spilts a render pass into one or more subpasses. Each render pass has to has at least one subpass.
        // In our case, our color attachment pass is our only rendering pass. (This will get extended with a depth & stencil attachment when we start doing shadows.)
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Depth attachment comes here.........


        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1; // Number of attachments.
        renderPassInfo.pAttachments = &colorAttachment; // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_Device->GetVKDevice(), &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            std::cerr << "Failed to create a render pass" << std::endl;
            __debugbreak();
        }
	}
    RenderPass::~RenderPass()
    {
        vkDestroyRenderPass(m_Device->GetVKDevice(), m_RenderPass, nullptr);
    }
}