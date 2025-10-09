#pragma once
#include "core.h"
// External
#include "vulkan/vulkan.h"
#include "glm/glm.hpp"
#include <vector>
#include <array>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
namespace OVK
{
	class Window;
	class Instance;
	class PhysicalDevice;
	class Surface;
	class LogicalDevice;
	class Swapchain;
	class Camera;
	class VulkanApplication
	{
	public:
		static VkFormat FindSupportedFormat(const Ref<PhysicalDevice>& physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		void			Run();
		float			DeltaTime();
	public:
		VulkanApplication(uint32_t framesInFlight);
		~VulkanApplication();
	public:
		// Vulkan components
		static inline Ref<Window>		  s_Window;
		static inline Ref<Instance>		  s_Instance;
		static inline Ref<Surface>		  s_Surface;
		static inline Ref<LogicalDevice>  s_Device;
		static inline Ref<PhysicalDevice> s_PhysicalDevice;
		static inline Ref<Camera>		  s_Camera;
		static inline Ref<Swapchain>	  s_Swapchain;
										  
		static inline uint64_t			  s_GraphicsQueueFamily = -1;
		static inline uint64_t			  s_ComputeQueueFamily = -1;
		static inline uint64_t			  s_TransferQueueFamily = -1;
	public:
		virtual void			OnUpdate() = 0;
		virtual void			OnStart() = 0;
		virtual void			OnWindowResize() = 0;
		virtual void			OnCleanup() = 0;
		virtual void			OnVulkanInit() = 0;
	public:
		float					GetRenderTime();
		inline void				SetDeviceExtensions(std::vector<const char*> extensions) { m_DeviceExtensions = extensions; }
		inline void				SetInstanceExtensions(std::vector<const char*> extensions) { m_InstanceExtensions = extensions; }
		inline void				SetCameraConfiguration(float FOV, float nearClip, float farClip) { m_CamFOV = FOV; m_CamNearClip = nearClip; m_CamFarClip = farClip; }
		inline uint32_t			CurrentFrameIndex() { return m_CurrentFrame; }
		static inline uint32_t  GetActiveImageIndex() { return m_ActiveImageIndex; }
		void					SubmitCommandBuffer(VkCommandBuffer& cmdBuffer);
	private:
		void Init();
		void InitImGui();
		void SetupQueueFamilies();
		void CreateSynchronizationPrimitives();
		VkSampleCountFlagBits GetMaxUsableSampleCount(const Ref<PhysicalDevice>& physDevice);
	private:
		float			m_Time = 0.0f;
		float			m_LastFrameRenderTime;

		// User defined variables.
		std::vector<const char*> m_DeviceExtensions;
		std::vector<const char*> m_InstanceExtensions;

		VkSampleCountFlagBits    m_MSAA;

		uint32_t m_FramesInFlight;
		uint32_t m_CurrentFrame = 0;
		std::vector<VkSemaphore> m_RenderingCompleteSemaphores;
		std::vector<VkSemaphore> m_ImageAvailableSemaphores;
		std::vector<VkFence>	 m_InFlightFences;
		VkCommandBuffer*		 m_CommandBufferReference; // These cmd buffers come from the client / user side. Probably will change later.

		static uint32_t				 m_ActiveImageIndex;

		VkDescriptorPool imguiPool;
		ImGui_ImplVulkan_InitInfo           init_info;


		float m_CamFOV = -1;
		float m_CamNearClip = -1;
		float m_CamFarClip = -1;

	};
}