#pragma once
#include "vulkan/vulkan.h"
namespace Anor
{
	class LogicalDevice;
	class RenderPass;
	class Framebuffer
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			RenderPass*    pRenderPass;
			uint32_t	   AttachmentCount;
			VkImageView*   pAttachments;
			uint32_t	   ExtentWidth;
			uint32_t	   ExtentHeight;
		};
	public:
		Framebuffer(CreateInfo& createInfo);
		~Framebuffer();
		VkFramebuffer GetVKFramebuffer() { return m_Framebuffer; }
	private:
		VkFramebuffer  m_Framebuffer = VK_NULL_HANDLE;
		LogicalDevice* m_Device;
		RenderPass*    m_RenderPass;
	};
}