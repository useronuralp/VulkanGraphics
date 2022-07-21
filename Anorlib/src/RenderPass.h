#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
namespace Anor
{
	class RenderPass
	{
	public:
		const VkRenderPass& GetRenderPass() { return m_RenderPass; }
		RenderPass(const VkFormat& depthFormat);
		~RenderPass();
	private:
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	};
}