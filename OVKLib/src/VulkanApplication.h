#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
#include "glm/glm.hpp"
namespace Anor
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
		VulkanApplication() {} // Doesn't do anything right now.
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
		static inline uint64_t			  s_PresentQueueFamily = -1;
		static inline uint64_t			  s_ComputeQueueFamily = -1;
	public:
		virtual void	OnUpdate() = 0;
		virtual void	OnStart() = 0;
		virtual void	OnWindowResize() = 0;
		virtual void	OnCleanup() = 0;
		virtual void	OnVulkanInit() = 0;
	public:
		float			GetRenderTime();
		inline void		SetDeviceExtensions(std::vector<const char*> extensions) { m_DeviceExtensions = extensions; }
		inline void		SetInstanceExtensions(std::vector<const char*> extensions) { m_InstanceExtensions = extensions; }
		inline void		SetCameraConfiguration(float FOV, float nearClip, float farClip) { m_CamFOV = FOV; m_CamNearClip = nearClip; m_CamFarClip = farClip; }
	private:
		void Init();
		void SetupQueueFamilies();
		VkSampleCountFlagBits GetMaxUsableSampleCount(const Ref<PhysicalDevice>& physDevice);
	private:
		float			m_Time = 0.0f;
		float			m_LastFrameRenderTime;

		// User defined variables.
		std::vector<const char*> m_DeviceExtensions;
		std::vector<const char*> m_InstanceExtensions;

		VkSampleCountFlagBits    m_MSAA;


		float m_CamFOV;
		float m_CamNearClip;
		float m_CamFarClip;

	};
}