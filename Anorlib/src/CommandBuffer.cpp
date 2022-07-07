#include "CommandBuffer.h"
#include "CommandPool.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffer.h"
#include "Buffer.h"
#include "DescriptorSet.h"
namespace Anor
{
    CommandBuffer::CommandBuffer(CommandPool* pool, LogicalDevice* device, DescriptorSet* dscSet)
        :m_Device(device), m_DscSet(dscSet), m_CommandPool(pool)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool->GetCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
    
        if (vkAllocateCommandBuffers(m_Device->GetVKDevice(), &allocInfo, &m_CommandBuffer) != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate command buffers" << std::endl;
            __debugbreak();
        }
    }
    void CommandBuffer::RecordDrawingCommandBuffer(uint32_t imageIndex, RenderPass& renderPass, Swapchain& swapchain, Pipeline& pipeline, Framebuffer& framebuffer, VertexBuffer& vertexBuffer, IndexBuffer& indexBuffer)
    {
        if (m_DscSet == nullptr)
        {
            std::cerr << "A descriptor set is not attached to the command buffer but trying to record one." << std::endl;
            __debugbreak();
        }
        Begin();

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass.GetRenderPass();
        renderPassInfo.framebuffer = framebuffer.GetVKFramebuffer();

        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = swapchain.GetExtent();

        VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // Record the following commands into the first parameter of each of those functions "commandBuffer"
        vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetVKPipeline());

        VkBuffer vertexBuffers[] =
        {
            vertexBuffer.GetVKBuffer()
        };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(m_CommandBuffer, indexBuffer.GetVKBuffer(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 0, 1, &m_DscSet->GetVKDescriptorSet(), 0, nullptr);
        vkCmdDrawIndexed(m_CommandBuffer, static_cast<uint32_t>(indexBuffer.GetIndices().size()), 1, 0, 0, 0);
        //vkCmdDraw(m_CommandBuffer, static_cast<uint32_t>(vertexBuffer.GetVertices().size()), 1, 0, 0);
        vkCmdEndRenderPass(m_CommandBuffer);

        End();
    }
    void CommandBuffer::ResetCommandBuffer()
    {
        vkResetCommandBuffer(m_CommandBuffer, 0);
    }
    void CommandBuffer::BeginSingleTimeCommands()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS)
        {
            std::cerr << "Failed to begin recording command buffer" << std::endl;
            __debugbreak();
        }
    }
    void CommandBuffer::EndnSingleTimeCommands()
    {
        if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS)
        {
            std::cerr << "Failed to record command buffer" << std::endl;
            __debugbreak();
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffer;

        vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Device->GetGraphicsQueue());

        vkFreeCommandBuffers(m_Device->GetVKDevice(), m_CommandPool->GetCommandPool(), 1, &m_CommandBuffer);
    }
    void CommandBuffer::Begin()
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(m_CommandBuffer, &beginInfo) != VK_SUCCESS)
        {
            std::cerr << "Failed to begin recording command buffer" << std::endl;
            __debugbreak();
        }
    }
    void CommandBuffer::End()
    {
        if (vkEndCommandBuffer(m_CommandBuffer) != VK_SUCCESS)
        {
            std::cerr << "Failed to record command buffer" << std::endl;
            __debugbreak();
        }
    }
}
