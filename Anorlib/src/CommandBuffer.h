#pragma once
#include "vulkan/vulkan.hpp"
#include "core.h"
#include <glm/glm.hpp>
namespace Anor
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
		static void Begin(const VkCommandBuffer& cmdBuffer);
		static void End(const VkCommandBuffer& cmdBuffer);
		static void BeginRenderPass(const VkCommandBuffer& cmdBuffer, const Ref<RenderPass>& renderPass, const Ref<Framebuffer>& framebuffer);
		static void BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderingInfoKHR& renderingInfo);
		static void BeginCustomRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderPassBeginInfo& renderPassBeginInfo);
		static void EndRenderPass(const VkCommandBuffer& cmdBuffer);
		static void EndRendering(const VkCommandBuffer& cmdBuffer);
		static void BindPipeline(const VkCommandBuffer& cmdBuffer, const Ref<Pipeline>& pipeline, const VkDeviceSize& offset);
		static void BindVertexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& VBO, const VkDeviceSize& offset);
		static void BindIndexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& IBO);
		static void BindDescriptorSet(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout, const Ref<DescriptorSet>& descriptorSet);
		static void DrawIndexed(const VkCommandBuffer& cmdBuffer, uint32_t indicesCount);
		static void Draw(const VkCommandBuffer& cmdBuffer, uint32_t vertexCount);
		static void SetViewport(const VkCommandBuffer& cmdBuffer, const VkViewport& viewport);
		static void SetScissor(const VkCommandBuffer& cmdBuffer, const VkRect2D& scissor);
		static void SetDepthBias(const VkCommandBuffer& cmdBuffer, float depthBiasConstant, float depthBiasSlope);
		static void Submit(const VkCommandBuffer& cmdBuffer);
		static void FreeCommandBuffer(const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool);
		static void Reset(const VkCommandBuffer& cmdBuffer);
	};
}