#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
	class GLFWwindow;
namespace Anor 
{
	class Framebuffer;
	class RenderPass;
	class Swapchain
	{
	public:
		Swapchain();
		~Swapchain();
	public:
		const VkSwapchainKHR&					GetVKSwapchain()						{ return m_Swapchain;			 }
		const VkFormat&							GetSwapchainImageFormat()				{ return m_SwapchainImageFormat; }
		const VkPresentModeKHR&					GetPresentMode()						{ return m_PresentMode;			 }
		std::vector<VkImage>&					GetSwapchainImages()					{ return m_SwapchainImages;		 }
		std::vector<VkImageView>&				GetSwapchainImageViews()				{ return m_ImageViews;			 }
		Ref<RenderPass>							GetSwapchainRenderPass()				{ return m_RenderPass;			 }
		VkImageView&							GetDepthImageView()						{ return m_DepthImageView;		 }
		const std::vector<Ref<Framebuffer>>&	GetFramebuffers()						{ return m_Framebuffers;		 }
		VkFormat								GetDepthFormat()						{ return m_DepthBufferFormat;	 }
		VkImage&								GetDepthImage()							{ return m_DepthImage;			 }
		void									OnResize();
	private:
		void	 Init();
		void	 CleanupSwapchain();
		VkFormat FindDepthFormat();
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		bool	 HasStencilComponent(VkFormat format);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	private:
		VkSwapchainKHR				    m_Swapchain = VK_NULL_HANDLE;

		// Variables needed for images / framebuffers.
		std::vector<Ref<Framebuffer>>	m_Framebuffers;
		Ref<RenderPass>					m_RenderPass;
		std::vector<VkImage>		    m_SwapchainImages;
		std::vector<VkImageView>		m_ImageViews;
		VkFormat						m_ImageFormat;
		uint32_t						m_ImageCount;

		// Variables needed for the depth buffer.
		VkImage							m_DepthImage;
		VkDeviceMemory					m_DepthImageMemory;
		VkImageView						m_DepthImageView;
		VkFormat						m_DepthBufferFormat;

		// Misc.
		VkFormat					    m_SwapchainImageFormat;
		VkPresentModeKHR				m_PresentMode;
	};
}