#include "Buffer.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "PhysicalDevice.h"
#include "Utils.h"
#include "VulkanContext.h"

#include <iostream>
#include <stb_image.h>

VertexBuffer::VertexBuffer(const std::vector<float>& vertices) : m_Vertices(vertices)
{
    VkDeviceSize bufferSize = m_Vertices.size() * sizeof(float);
    // The buffer we create on host side.
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Utils::CreateVKBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, &memRequirements);
    memcpy_s(
        data,
        memRequirements.size,
        m_Vertices.data(),
        (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped
                             // "data" pointer.
    vkUnmapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    // The following buffer is not visible to CPU.
    Utils::CreateVKBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}

VertexBuffer::VertexBuffer(const float* vertices, size_t bufferSize)
{
    // The buffer we create on host side.
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Utils::CreateVKBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, &memRequirements);
    memcpy_s(
        data,
        memRequirements.size,
        vertices,
        (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped
                             // "data" pointer.
    vkUnmapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    // The following buffer is not visible to CPU.
    Utils::CreateVKBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}

VertexBuffer::~VertexBuffer()
{
    vkDestroyBuffer(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_Buffer, nullptr);
    vkFreeMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BufferMemory, nullptr);
}

IndexBuffer::IndexBuffer(const std::vector<uint32_t>& indices) : m_Indices(indices)
{
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    Utils::CreateVKBuffer(
        bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), (size_t)bufferSize);
    vkUnmapMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}
IndexBuffer::~IndexBuffer()
{
    vkDestroyBuffer(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_Buffer, nullptr);
    vkFreeMemory(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BufferMemory, nullptr);
}
