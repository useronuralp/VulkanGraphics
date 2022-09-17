#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace OVK
{
	class Texture;
	class Framebuffer
	{
	public:
		Framebuffer(const VkRenderPass& renderPass, std::vector<VkImageView> attachments);
		Framebuffer(const VkRenderPass& renderPass, const Ref<Texture>& texture);
		~Framebuffer();
		const uint32_t& GetWidth()  { return m_Width;  }
		const uint32_t& GetHeight() { return m_Height; }
		const VkFramebuffer& GetVKFramebuffer() { return m_Framebuffer; }
	private:
		VkFramebuffer		m_Framebuffer = VK_NULL_HANDLE;
		uint32_t m_Width;
		uint32_t m_Height;
	};
}