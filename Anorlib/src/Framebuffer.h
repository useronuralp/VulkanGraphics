#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace Anor
{
	class LogicalDevice;
	class RenderPass;
	class Surface;
	class Framebuffer
	{
	public:
		Framebuffer(const Ref<LogicalDevice>& device, const Ref<RenderPass>& renderPass, const Ref<Surface>& surface, std::vector<VkImageView> attachments);
		~Framebuffer();
		const VkFramebuffer& GetVKFramebuffer() { return m_Framebuffer; }
	private:
		VkFramebuffer		m_Framebuffer = VK_NULL_HANDLE;
		Ref<LogicalDevice>	m_Device;
		Ref<RenderPass>		m_RenderPass;
	};
}