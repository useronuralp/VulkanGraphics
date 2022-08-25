#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
#include "glm/glm.hpp"
#include "RenderPass.h"
namespace Anor
{
	class Window;
	class Instance;
	class PhysicalDevice;
	class Surface;
	class LogicalDevice;
	class RenderPass;
	class Swapchain;
	class Camera;
	class Framebuffer;
	class VulkanApplication
	{
	public:
		static VkFormat FindSupportedFormat(const Ref<PhysicalDevice>& physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		void			Run();
		float			DeltaTime();
	public:
		VulkanApplication();
	public:
		// Vulkan components
		static inline Ref<Window>		  s_Window;
		static inline Ref<Instance>		  s_Instance;
		static inline Ref<Surface>		  s_Surface;
		static inline Ref<LogicalDevice>  s_Device;
		static inline Ref<PhysicalDevice> s_PhysicalDevice;
		static inline Ref<Camera>		  s_Camera;
		static inline Ref<Swapchain>	  s_Swapchain;
										  
		static inline uint64_t			  s_GraphicsANDComputeQueueIndex = -1;
		static inline uint64_t			  s_PresentQueueIndex = -1;
	public:
		/// <summary>
		/// Code that needs to be updated every frame goes into this function.
		/// </summary>
		virtual void			OnUpdate() = 0;
		/// <summary>
		/// Code that is wanted to be called only once before stepping into the main loop goes into this function.
		/// E.g: Model loading, buffer initializations and etc.
		/// </summary>
		virtual void			OnStart() = 0;
		/// <summary>
		/// Code that is wanted to be called when window resizes goes in here. (Usually in the form of "Model / Mesh Name->OnResize()" )
		/// </summary>
		virtual void			OnWindowResize() = 0;
		/// <summary>
		/// Cleanup code goes in here. Used mainly for deleting pointers.
		/// </summary>
		virtual void			OnCleanup() = 0;
		/// <summary>
		/// In this function, the configuration of the swapchain render pass needs to be specified. 
		/// </summary>
	public:
		/// <summary>
		/// Calls the glfwGetTime() at the lowest level.
		/// </summary>
		/// <returns>The time it took to render the last frame. Can be used to calculate delta time.</returns>
		float					GetRenderTime();
		/// <summary>
		/// Can be used to pass to model / mesh shaders.
		/// </summary>
		/// <returns>View matrix of the scene camera.</returns>
		glm::mat4				GetCameraViewMatrix();
		/// <summary>
		/// Can be passed to shaders.
		/// </summary>
		/// <returns>Projection matrix of the camera.</returns>
		glm::mat4				GetCameraProjectionMatrix();
		/// <returns>The current camera position in the world space.</returns>
		glm::vec3				GetCameraPosition();
		/// <returns>Returns the latest recorded command buffer.</returns>
		const VkCommandBuffer&	GetActiveCommandBuffer() const { return m_ActiveCommandBuffer; }
		/// <summary>
		/// Must be called before attempting to draw anything on the screen. In Vulkan we record drawing commands into command buffers before we can use them.
		/// This function marks the beginning of a render pass THAT records a command buffer in its essence, therefore, also marking the start of a command buffer recording operation.
		/// </summary>
		void					BeginRenderPass();
		/// <summary>
		/// Must be called when a render pass is finished being recorded. This function marks the end of a render pass and therefore, also the end of a command buffer recording.
		/// </summary>
		void					BeginCustomRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo);
		void					BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderingInfoKHR& renderingInfo);
		void					EndDepthPass();
		void EndRendering();
		void					SetViewport(const VkViewport& viewport);
		void					SetScissor(const VkRect2D& scissor);
		void					SetDepthBias(float depthBiasConstant, float slopeBias);
		void					EndRenderPass();
		static VkDevice			GetVKDevice();
	private:
		
		VkCommandBuffer	m_ActiveCommandBuffer		 = VK_NULL_HANDLE;
		VkCommandPool	m_ActiveCommandPool			 = VK_NULL_HANDLE;

		std::vector<VkCommandBuffer>				 m_CommandBuffers;
		std::vector<VkCommandPool>					 m_CommandPools;


		// Rendering semaphores
		VkSemaphore		m_ImageAvailableSemaphore	 = VK_NULL_HANDLE;
		VkSemaphore		m_RenderingCompleteSemaphore = VK_NULL_HANDLE;
		VkFence			m_InRenderingFence			 = VK_NULL_HANDLE;

		
		uint32_t		m_OutImageIndex;
		float			m_Time = 0.0f;
		float			m_LastFrameRenderTime;


	};
}