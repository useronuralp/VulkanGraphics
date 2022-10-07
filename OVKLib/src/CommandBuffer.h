#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
#include <glm/glm.hpp>
namespace OVK
{
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
		static void Create(uint32_t queueFamilyIndex, VkCommandPool& outCmdPool, VkCommandBuffer& outCmdBuffer);
		static void BeginRecording(const VkCommandBuffer& cmdBuffer);
		static void EndRecording(const VkCommandBuffer& cmdBuffer);
		static void BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderPassBeginInfo& renderPassBeginInfo, VkSubpassContents contents);
		static void EndRenderPass(const VkCommandBuffer& cmdBuffer);
		static void BindPipeline(const VkCommandBuffer& cmdBuffer, const Ref<Pipeline>& pipeline);
		static void BindVertexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& VBO, const VkDeviceSize& offset);
		static void BindIndexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& IBO);
		static void BindDescriptorSet(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout, const Ref<DescriptorSet>& descriptorSet);
		static void DrawIndexed(const VkCommandBuffer& cmdBuffer, uint32_t indicesCount);
		static void Draw(const VkCommandBuffer& cmdBuffer, uint32_t vertexCount);
		static void FreeCommandBuffer(const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool);
		static void Submit(const VkCommandBuffer& cmdBuffer);
		static void Reset(const VkCommandBuffer& cmdBuffer);
	};
}