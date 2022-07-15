#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "core.h"
	class GLFWwindow;
namespace Anor 
{
	class Framebuffer;
	class Instance;
	class LogicalDevice;
	class Surface;
	class RenderPass;
	class PhysicalDevice;
	class Swapchain
	{
	public:
		Swapchain(const Ref<PhysicalDevice>& physDevice, const Ref<LogicalDevice>& device, const Ref<Surface>& surface, const Ref<RenderPass>& renderPass, VkFormat depthFormat);
		~Swapchain();

		const VkSwapchainKHR&		GetVKSwapchain()						{ return m_Swapchain;			 }
		const VkFormat&				GetSwapchainImageFormat()				{ return m_SwapchainImageFormat; }
		const VkPresentModeKHR&		GetPresentMode()						{ return m_PresentMode;			 }
		const VkExtent2D&			GetExtent()								{ return m_SwapchainExtent;		 }
		const VkFence&				GetIsRenderingFence()			const	{ return m_InRenderingFence;	 }
		const VkSemaphore&			GetImageAvailableSemaphore()	const	{ return m_ImageAvailableSemaphore; }
		const VkSemaphore&			GetRenderingCompleteSemaphore()	const	{ return m_RenderingCompleteSemaphore; }
		uint32_t					GetActiveImageIndex()					{ return m_ActiveImageIndex;	 }
		std::vector<VkImage>&		GetSwapchainImages()					{ return m_SwapchainImages;		 }
		std::vector<VkImageView>&	GetSwapchainImageViews()				{ return m_ImageViews;			 }
		const std::vector<Ref<Framebuffer>>&	GetFramebuffers()						{ return m_Framebuffers; }
		uint32_t					AcquireNextImage();
		void						ResetFence();

	private:
		VkFormat FindDepthFormat();
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		bool HasStencilComponent(VkFormat format);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	private:
		VkSwapchainKHR				    m_Swapchain = VK_NULL_HANDLE;
		Ref<PhysicalDevice>				m_PhysicalDevice;
		Ref<LogicalDevice>				m_LogicalDevice;
		Ref<Surface>					m_Surface;
		Ref<RenderPass>					m_RenderPass;

		// Variables needed for images / framebuffers.
		std::vector<Ref<Framebuffer>>	m_Framebuffers;
		std::vector<VkImage>		    m_SwapchainImages;
		std::vector<VkImageView>		m_ImageViews;

		// Variables needed for the depth buffer.
		VkImage							m_DepthImage;
		VkDeviceMemory					m_DepthImageMemory;
		VkImageView						m_DepthImageView;

		VkFormat					    m_SwapchainImageFormat;
		VkFormat						m_DepthBufferFormat;
		uint32_t						m_ActiveImageIndex = -1;
		VkPresentModeKHR				m_PresentMode;
		VkExtent2D						m_SwapchainExtent;

		VkSemaphore						m_ImageAvailableSemaphore	 = VK_NULL_HANDLE;
		VkSemaphore						m_RenderingCompleteSemaphore = VK_NULL_HANDLE;
		VkFence							m_InRenderingFence			 = VK_NULL_HANDLE;
	};
}