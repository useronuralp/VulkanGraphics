#pragma once
#include "vulkan/vulkan.hpp"
namespace Anor
{
	class CommandPool;
	class LogicalDevice;
	class RenderPass;
	class Swapchain;
	class Framebuffer;
	class Pipeline;
	class VertexBuffer;
	class IndexBuffer;
	class DescriptorSet;
	class CommandBuffer
	{
	public:
		CommandBuffer(CommandPool* pool, LogicalDevice* device, DescriptorSet* dscSet = nullptr);
		void RecordDrawingCommandBuffer(uint32_t imageIndex, RenderPass& renderPass, Swapchain& swapchain, Pipeline& pipeline, Framebuffer& framebuffer, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer);
		void ResetCommandBuffer();
		const VkCommandBuffer& GetVKCommandBuffer() { return m_CommandBuffer; }
		void BeginSingleTimeCommands();
		void EndnSingleTimeCommands();
	private:
		void Begin();
		void End();
	private:
		VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		LogicalDevice* m_Device;
		DescriptorSet* m_DscSet;
		CommandPool* m_CommandPool;
	};
}