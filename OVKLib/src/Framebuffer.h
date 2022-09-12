#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
namespace OVK
{
	class LogicalDevice;
	class Texture;
	class RenderPass;
	class Surface;
	class Framebuffer
	{
	public:
		Framebuffer(const Ref<RenderPass>& renderPass, std::vector<VkImageView> attachments);
		Framebuffer(const Ref<RenderPass>& renderPass, const Ref<Texture>& texture);
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