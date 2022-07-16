#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
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
	class ImageBuffer;
	class VulkanApplication
	{
	public:
		static VkFormat FindSupportedFormat(const Ref<PhysicalDevice>& physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		VulkanApplication();
		//Ref<VertexBuffer>  CreateVertexBuffer(const std::vector<Anor::Vertex> vertices);
		//Ref<IndexBuffer>   CreateIndexBuffer(const std::vector<uint64_t> indices);
		//Ref<UniformBuffer> CreateUniformBuffer(uint64_t allocateSize);
		//Ref<ImageBuffer>   CreateImageBuffer(const char* texturePath);
		void Run();
	private:
		// Vulkan components
		Ref<Window>			 m_Window;
		Ref<Instance>		 m_Instance;
		Ref<Surface>		 m_Surface;
		Ref<LogicalDevice>	 m_Device;
		Ref<PhysicalDevice>  m_PhysicalDevice;
		Ref<RenderPass>		 m_RenderPass;
		Ref<Swapchain>		 m_Swapchain;
		Ref<DescriptorSet>   m_DescriptorSet;
		Ref<CommandBuffer>   m_CommandBuffer;
		Ref<Pipeline>		 m_Pipeline;
		Ref<VertexBuffer>	 m_VBO;
		Ref<IndexBuffer>	 m_IBO;
		Ref<UniformBuffer>	 m_UBO;
		Ref<ImageBuffer>	 m_ImBO;
		// Queue Indices
		uint64_t			m_GraphicsQueueIndex = -1;
		uint64_t			m_PresentQueueIndex = -1;

		VkFormat			m_DepthFormat; 
	};
}