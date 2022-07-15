#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
namespace Anor
{
	class LogicalDevice;
	class RenderPass
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			VkFormat	   ColorAttachmentFormat;
			VkFormat	   DepthAttachmentFormat;
		};
	public:
		const VkRenderPass& GetRenderPass() { return m_RenderPass; }
		RenderPass(const Ref<LogicalDevice>& device, VkFormat colorFormat, VkFormat depthFormat);
		~RenderPass();
	private:
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		Ref<LogicalDevice> m_Device;
	};
}