#include "CommandBuffer.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Surface.h"
#include <iostream>
namespace Anor
{
    void CommandBuffer::Create(const Ref<LogicalDevice>& device, uint32_t queueFamilyIndex, VkCommandPool& outCmdPool, VkCommandBuffer& outCmdBuffer)
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        
        ASSERT(vkCreateCommandPool(device->GetVKDevice(), &poolInfo, nullptr, &outCmdPool) == VK_SUCCESS, "Failed to create command pool!");

        // Allocate the memory for the command buffer.
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = outCmdPool;
        allocInfo.commandBufferCount = 1;

        ASSERT(vkAllocateCommandBuffers(device->GetVKDevice(), &allocInfo, &outCmdBuffer) == VK_SUCCESS, "Failed to allocate command buffer memory");
    }
    void CommandBuffer::EndSingleTimeCommandBuffer(const Ref<LogicalDevice>& device, const VkCommandBuffer& cmdBuffer, const VkCommandPool& cmdPool)
    {
        ASSERT(vkEndCommandBuffer(cmdBuffer) == VK_SUCCESS, "Failed to record command buffer");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        vkQueueSubmit(device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(device->GetGraphicsQueue());
        vkFreeCommandBuffers(device->GetVKDevice(), cmdPool, 1, &cmdBuffer);
        vkDestroyCommandPool(device->GetVKDevice(), cmdPool, nullptr); // WARNING: This part could be problematic. Test.
    }
    void CommandBuffer::BeginSingleTimeCommandBuffer(const VkCommandBuffer& cmdBuffer)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr; // Optional

        ASSERT(vkBeginCommandBuffer(cmdBuffer, &beginInfo) == VK_SUCCESS, "Failed to begin recording command buffer");
    }
    CommandBuffer::CommandBuffer(const Ref<LogicalDevice>& device, uint32_t queueFamilyIndex, const Ref<DescriptorSet>& dscSet)
        :m_Device(device), m_DscSet(dscSet)
    {
        // Create the command pool to allocate the buffer from.
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndex;

        ASSERT(vkCreateCommandPool(m_Device->GetVKDevice(), &poolInfo, nullptr, &m_CommandPool) == VK_SUCCESS, "Failed to create command pool!");

        // Allocate the memory for the command buffer.
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;
    
        ASSERT(vkAllocateCommandBuffers(m_Device->GetVKDevice(), &allocInfo, &m_CommandBuffer) == VK_SUCCESS, "Failed to allocate command buffer memory");
    }
    CommandBuffer::~CommandBuffer()
    {
        vkDestroyCommandPool(m_Device->GetVKDevice(), m_CommandPool, nullptr);
    }
    void CommandBuffer::RecordDrawingCommandBuffer(const Ref<RenderPass>& renderPass, const Ref<Surface>& surface, const Ref<Pipeline>& pipeline, const Ref<Framebuffer>& framebuffer, const Ref<VertexBuffer>& vertexBuffer, const Ref<IndexBuffer>& indexBuffer)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        ASSERT(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) == VK_SUCCESS, "Failed to begin recording command buffer");

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass->GetRenderPass();
        renderPassInfo.framebuffer = framebuffer->GetVKFramebuffer();

        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = surface->GetVKExtent();

        // WARNING: The clear value order shoould be identical to your attachment orders.
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVKPipeline());

        VkBuffer vertexBuffers[] =
        {
            vertexBuffer->GetVKBuffer()
        };
        VkDeviceSize offsets[] = { 0 };
        
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(m_CommandBuffer, indexBuffer->GetVKBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &m_DscSet->GetVKDescriptorSet(), 0, nullptr);
        vkCmdDrawIndexed(m_CommandBuffer, static_cast<uint32_t>(indexBuffer->GetIndices().size()), 1, 0, 0, 0);
        //vkCmdDraw(m_CommandBuffer, static_cast<uint32_t>(vertexBuffer->GetVertices().size()), 1, 0, 0);
        vkCmdEndRenderPass(m_CommandBuffer);

        ASSERT(vkEndCommandBuffer(m_CommandBuffer) == VK_SUCCESS, "Failed to record command buffer");
    }
    void CommandBuffer::ResetCommandBuffer()
    {
        vkResetCommandBuffer(m_CommandBuffer, 0);
    }
}
