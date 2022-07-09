#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "PhysicalDevice.h"
#include "Window.h"
	class GLFWwindow;
namespace Anor 
{
	class Framebuffer;
	class Instance;
	class LogicalDevice;
	class Surface;
	class RenderPass;
	class Swapchain
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			PhysicalDevice* pPhysicalDevice;
			Surface* pSurface;
			RenderPass* pRenderPass;
		};
		Swapchain(CreateInfo& createInfo);
		~Swapchain();

		VkSwapchainKHR&				GetVKSwapchain()						{ return m_Swapchain;			 }
		std::vector<VkImageView>&	GetSwapchainImageViews()				{ return m_ImageViews;			 }
		VkFormat&					GetSwapchainImageFormat()				{ return m_SwapchainImageFormat; }
		std::vector<VkImage>&		GetSwapchainImages()					{ return m_SwapchainImages;		 }
		uint32_t					GetActiveImageIndex()					{ return m_ActiveImageIndex;	 }
		VkPresentModeKHR&			GetPresentMode()						{ return m_PresentMode;			 }
		VkExtent2D&					GetExtent()								{ return m_SwapchainExtent;		 }
		const VkFence&				GetIsRenderingFence()			const	{ return m_InRenderingFence;	 }
		const VkSemaphore&			GetImageAvailableSemaphore()	const	{ return m_ImageAvailableSemaphore; }
		const VkSemaphore&			GetRenderingCompleteSemaphore()	const	{ return m_RenderingCompleteSemaphore; }
		uint32_t					AcquireNextImage();
		std::vector<Framebuffer*>&	GetFramebuffers() { return m_Framebuffers; }
		void						ResetFence();

		VkResult QueuePresent(const VkQueue& presentQueue, const VkSemaphore &waitSemaphore);
	private:
		VkFormat FindDepthFormat();
		VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		bool HasStencilComponent(VkFormat format);
		uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	private:
		VkSwapchainKHR				    m_Swapchain = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		PhysicalDevice* m_PhysicalDevice;
		LogicalDevice*  m_LogicalDevice;
		Surface*		m_Surface;
		RenderPass*		m_RenderPass;

		std::vector<Framebuffer*>	    m_Framebuffers;
		std::vector<VkImage>		    m_SwapchainImages;
		std::vector<VkImageView>		m_ImageViews;

		VkImage							m_DepthImage;
		VkDeviceMemory					m_DepthImageMemory;
		VkImageView						m_DepthImageView;

		VkFormat					    m_SwapchainImageFormat;
		uint32_t						m_ActiveImageIndex = -1;
		VkPresentModeKHR				m_PresentMode;
		VkExtent2D						m_SwapchainExtent;

		VkSemaphore						m_ImageAvailableSemaphore	 = VK_NULL_HANDLE;
		VkSemaphore						m_RenderingCompleteSemaphore = VK_NULL_HANDLE;
		VkFence							m_InRenderingFence			 = VK_NULL_HANDLE;
	};
}