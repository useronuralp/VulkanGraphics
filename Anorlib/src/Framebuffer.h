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
		Framebuffer(const Ref<RenderPass>& renderPass, std::vector<VkImageView> attachments, uint32_t width = UINT32_MAX, uint32_t height = UINT32_MAX);
		~Framebuffer();
		uint32_t GetWidth() { return m_Width; }
		uint32_t GetHeight() { return m_Height; }
		const VkFramebuffer& GetVKFramebuffer() { return m_Framebuffer; }
	private:
		VkFramebuffer		m_Framebuffer = VK_NULL_HANDLE;

		uint32_t m_Width;
		uint32_t m_Height;
	};
}