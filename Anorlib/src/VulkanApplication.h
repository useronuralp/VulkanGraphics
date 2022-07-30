#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
#include "glm/glm.hpp"
namespace Anor
{
	class Window;
	class Instance;
	class IndexBuffer;
	class VertexBuffer;
	class UniformBuffer;
	class PhysicalDevice;
	class Surface;
	class LogicalDevice;
	class RenderPass;
	class CommandBuffer;
	class Swapchain;
	struct Vertex;
	class DescriptorSet;
	class Pipeline;
	class Camera;
	class ImageBuffer;
	class Model;
	class Mesh;
	class VulkanApplication
	{
	public:
		static VkFormat FindSupportedFormat(const Ref<PhysicalDevice>& physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VulkanApplication();
		void Run();
		float DeltaTime();
	public:
		// Vulkan components
		static inline Ref<Window>			 s_Window;
		static inline Ref<Instance>			 s_Instance;
		static inline Ref<Surface>			 s_Surface;
		static inline Ref<LogicalDevice>	 s_Device;
		static inline Ref<PhysicalDevice>	 s_PhysicalDevice;
		static inline Ref<RenderPass>		 s_ModelRenderPass;
		static inline Ref<Camera>			 s_Camera;

		static inline uint64_t				 s_GraphicsQueueIndex = -1;
		static inline uint64_t				 s_PresentQueueIndex = -1;
	private:
		Ref<Swapchain>		 m_Swapchain;
		Ref<CommandBuffer>   m_CommandBuffer;


		VkFormat			m_DepthFormat; 
		float				m_Time = 0.0f;
		float				m_LastFrameRenderTime;

		Model* m_Model;
		Model* m_LightSphere;


		// Semaphores
		VkSemaphore						m_ImageAvailableSemaphore = VK_NULL_HANDLE;
		VkSemaphore						m_RenderingCompleteSemaphore = VK_NULL_HANDLE;
		VkFence							m_InRenderingFence = VK_NULL_HANDLE;
	};
}