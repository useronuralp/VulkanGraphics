#pragma once
#include "core.h"
// External
#include <vulkan/vulkan.h>
#include <vector>
	class GLFWwindow;
namespace OVK 
{
	class Framebuffer;
	class Swapchain
	{
	public:
		Swapchain();
		~Swapchain();
	public:
		const VkSwapchainKHR&					GetVKSwapchain()			const	{ return m_Swapchain;			 }
		const VkFormat&							GetSwapchainImageFormat()	const	{ return m_SwapchainImageFormat; }
		const VkPresentModeKHR&					GetPresentMode()			const	{ return m_PresentMode;			 }
		const std::vector<VkImage>&				GetSwapchainImages()		const	{ return m_SwapchainImages;		 }
		const std::vector<VkImageView>&			GetSwapchainImageViews()	const	{ return m_ImageViews;			 }
		VkRenderPass&							GetSwapchainRenderPass()	 		{ return m_RenderPass;			 }
		const VkImageView&						GetDepthImageView()			const	{ return m_DepthImageView;		 }
		const std::vector<Ref<Framebuffer>>&	GetFramebuffers()			const	{ return m_Framebuffers;		 }
		const VkFormat&							GetDepthFormat()			const	{ return m_DepthBufferFormat;	 }
		const VkImage&							GetDepthImage()				const	{ return m_DepthImage;			 }
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
		VkRenderPass					m_RenderPass;
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