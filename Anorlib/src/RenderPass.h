#pragma once
#include "vulkan/vulkan.h"
namespace Anor
{
	class LogicalDevice;
	class RenderPass
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			VkFormat	   Format;
		};
	public:
		VkRenderPass& GetRenderPass() { return m_RenderPass; }
		RenderPass(CreateInfo& createInfo);
		~RenderPass();
	private:
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
		LogicalDevice* m_Device;
	};
}