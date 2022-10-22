#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace OVK
{
	class Framebuffer
	{
	public:
		Framebuffer() = default;
		Framebuffer(const VkRenderPass& renderPass, std::vector<VkImageView> attachments, uint32_t width, uint32_t height);
		~Framebuffer();
		const uint32_t&			GetWidth()  { return m_Width;  }
		const uint32_t&			GetHeight() { return m_Height; }
		const VkFramebuffer&	GetVKFramebuffer() { return m_Framebuffer; }
	private:
		VkFramebuffer		m_Framebuffer = VK_NULL_HANDLE;
		uint32_t m_Width;
		uint32_t m_Height;
	};
}