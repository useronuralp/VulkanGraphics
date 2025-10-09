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
		static void CreateCommandBuffer(VkCommandBuffer& outCmdBuffer, const VkCommandPool& poolToAllocateFrom);
		static void CreateCommandBufferPool(uint32_t queueFamilyIndex, VkCommandPool& outCmdPool);
		static void BeginRecording(const VkCommandBuffer& cmdBuffer);
		static void EndRecording(const VkCommandBuffer& cmdBuffer);
		static void BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderPassBeginInfo& renderPassBeginInfo, VkSubpassContents contents);
		static void EndRenderPass(const VkCommandBuffer& cmdBuffer);
		static void BindPipeline(const VkCommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint, const Ref<Pipeline>& pipeline);
		static void BindVertexBuffer(const VkCommandBuffer& cmdBuffer, uint32_t firstBinding, uint32_t bindingCount, const VkBuffer& VBO, const VkDeviceSize& offset);
		static void BindIndexBuffer(const VkCommandBuffer& cmdBuffer, uint32_t offset, const VkBuffer& IBO, VkIndexType indexType);
		//static void BindDescriptorSets(const VkCommandBuffer& cmdBuffer, VkPipelineBindPoint bindPoint, uint32_t firstSet, uint32_t descCount, const VkPipelineLayout& pipelineLayout, const Ref<DescriptorSet>& descriptorSet,
		//	uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets);
		static void DrawIndexed(const VkCommandBuffer& cmdBuffer, uint32_t indicesCount, uint32_t instanceCount, uint32_t firstIndex, uint32_t vertexOffset, uint32_t firstInstance);
		static void Draw(const VkCommandBuffer& cmdBuffer, uint32_t vertexCount);
		static void FreeCommandBuffer(const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool, const VkQueue& queueToWaitFor);
		static void Submit(const VkCommandBuffer& cmdBuffer, const VkQueue& queue);
		static void Reset(const VkCommandBuffer& cmdBuffer);
		static void DestroyCommandPool(const VkCommandPool& pool);
		static void PushConstants(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout, VkShaderStageFlags shaderStage, uint32_t offset, uint32_t size, const void* pValues);
	};
}