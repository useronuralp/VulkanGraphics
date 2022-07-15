#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
namespace Anor
{
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
		static void Create(const Ref<LogicalDevice>& device, uint32_t queueFamilyIndex, VkCommandPool& outCmdPool, VkCommandBuffer& outCmdBuffer);
		static void BeginSingleTimeCommandBuffer(const VkCommandBuffer& cmdBuffer);
		static void EndSingleTimeCommandBuffer(const Ref<LogicalDevice>& device, const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool);
	public:
		CommandBuffer(const Ref<LogicalDevice>& device, uint32_t queueFamilyIndex, const Ref<DescriptorSet>& dscSet);
		~CommandBuffer();
		const VkCommandPool& GetCommandPool() { return m_CommandPool; }
		void RecordDrawingCommandBuffer(uint32_t imageIndex, const Ref<RenderPass>& renderPass, const Ref<Swapchain>& swapchain, const Ref<Pipeline>& pipeline, const Ref<Framebuffer>& framebuffer,
			const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer);
		void ResetCommandBuffer();
		const VkCommandBuffer& GetVKCommandBuffer() { return m_CommandBuffer; }
	private:
		VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
		VkCommandPool m_CommandPool;
		Ref<LogicalDevice> m_Device;
		Ref<DescriptorSet> m_DscSet;
	};
}