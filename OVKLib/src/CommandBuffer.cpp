#include "CommandBuffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Surface.h"
#include <iostream>
#include <chrono>
namespace OVK
{
    void CommandBuffer::CreateCommandBuffer(VkCommandBuffer& outCmdBuffer, const VkCommandPool& poolToAllocateFrom)
    {
        // Allocate the memory for the command buffer.
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = poolToAllocateFrom;
        allocInfo.commandBufferCount = 1;

        ASSERT(vkAllocateCommandBuffers(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &outCmdBuffer) == VK_SUCCESS, "Failed to allocate command buffer memory");
    }
    void CommandBuffer::CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPool& outCmdPool)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex; // The command buffer created from this command pool must be submitted to a queue allocated from this queueFamilyIndex.
        
        ASSERT(vkCreateCommandPool(VulkanApplication::s_Device->GetVKDevice(), &poolInfo, nullptr, &outCmdPool) == VK_SUCCESS, "Failed to create command pool!");
    }
    void CommandBuffer::BeginRecording(const VkCommandBuffer& cmdBuffer)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Optional

        ASSERT(vkBeginCommandBuffer(cmdBuffer, &beginInfo) == VK_SUCCESS, "Failed to begin recording command buffer");
    }
    void CommandBuffer::EndRecording(const VkCommandBuffer& cmdBuffer)
    {
        ASSERT(vkEndCommandBuffer(cmdBuffer) == VK_SUCCESS, "Failed to record command buffer");
    }
    void CommandBuffer::BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderPassBeginInfo& renderPassBeginInfo, VkSubpassContents contents)
    {
        vkCmdBeginRenderPass(cmdBuffer, &renderPassBeginInfo, contents);
    }
    void CommandBuffer::EndRenderPass(const VkCommandBuffer& cmdBuffer)
    {
        vkCmdEndRenderPass(cmdBuffer);
    }
    void CommandBuffer::BindPipeline(const VkCommandBuffer& cmdBuffer, const Ref<Pipeline>& pipeline)
    {
        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVKPipeline());
    }
    void CommandBuffer::BindVertexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& VBO, const VkDeviceSize& offset)
    {
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &VBO, &offset);
    }
    void CommandBuffer::BindIndexBuffer(const VkCommandBuffer& cmdBuffer, const VkBuffer& IBO)
    {
        vkCmdBindIndexBuffer(cmdBuffer, IBO, 0, VK_INDEX_TYPE_UINT32);
    }
    void CommandBuffer::BindDescriptorSet(const VkCommandBuffer& cmdBuffer, const VkPipelineLayout& pipelineLayout, const Ref<DescriptorSet>& descriptorSet)
    {
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet->GetVKDescriptorSet(), 0, nullptr);
    }
    void CommandBuffer::DrawIndexed(const VkCommandBuffer& cmdBuffer, uint32_t indicesCount)
    {
        vkCmdDrawIndexed(cmdBuffer, indicesCount, 1, 0, 0, 0);
    }
    void CommandBuffer::Draw(const VkCommandBuffer& cmdBuffer, uint32_t vertexCount)
    {
        vkCmdDraw(cmdBuffer, vertexCount, 1, 0, 0);
    }
    void CommandBuffer::FreeCommandBuffer(const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool, const VkQueue& queueToWaitFor)
    {
        vkQueueWaitIdle(queueToWaitFor);
        vkFreeCommandBuffers(VulkanApplication::s_Device->GetVKDevice(), cmdPool, 1, &cmdBuffer);
    }
    void CommandBuffer::Submit(const VkCommandBuffer& cmdBuffer, const VkQueue& queue)
    {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    }
    void CommandBuffer::Reset(const VkCommandBuffer& cmdBuffer)
    {
        vkDeviceWaitIdle(VulkanApplication::s_Device->GetVKDevice());
        vkResetCommandBuffer(cmdBuffer, 0);
    }
    void CommandBuffer::DestroyCommandPool(const VkCommandPool& pool)
    {
        vkDestroyCommandPool(VulkanApplication::s_Device->GetVKDevice(), pool, nullptr);
    }
}
