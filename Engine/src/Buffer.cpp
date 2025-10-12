#include "Buffer.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Engine.h"
#include "LogicalDevice.h"
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
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* data;
    vkMapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, &memRequirements);
    memcpy_s(
        data,
        memRequirements.size,
        m_Vertices.data(),
        (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped
                             // "data" pointer.
    vkUnmapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    // The following buffer is not visible to CPU.
    Utils::CreateVKBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Buffer,
        m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}

VertexBuffer::VertexBuffer(const float* vertices, size_t bufferSize)
{
    // The buffer we create on host side.
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    Utils::CreateVKBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* data;
    vkMapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, &memRequirements);
    memcpy_s(
        data,
        memRequirements.size,
        vertices,
        (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped
                             // "data" pointer.
    vkUnmapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    // The following buffer is not visible to CPU.
    Utils::CreateVKBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Buffer,
        m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}

VertexBuffer::~VertexBuffer()
{
    vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(), m_Buffer, nullptr);
    vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(), m_BufferMemory, nullptr);
}

IndexBuffer::IndexBuffer(const std::vector<uint32_t>& indices) : m_Indices(indices)
{
    VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();

    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    Utils::CreateVKBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory);

    void* data;
    vkMapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, m_Indices.data(), (size_t)bufferSize);
    vkUnmapMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory);

    Utils::CreateVKBuffer(
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        m_Buffer,
        m_BufferMemory);

    Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

    vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBuffer, nullptr);
    vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(), stagingBufferMemory, nullptr);
}
IndexBuffer::~IndexBuffer()
{
    vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(), m_Buffer, nullptr);
    vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(), m_BufferMemory, nullptr);
}
// UniformBuffer::UniformBuffer(const Ref<DescriptorSet>& dscSet, size_t
// allocationSize, uint32_t bindingIndex)
//{
//     VkDeviceSize bufferSize = allocationSize;
//     Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
//     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Buffer, m_BufferMemory);
//
//     VkDescriptorBufferInfo bufferInfo{};
//     bufferInfo.buffer = m_Buffer;
//     bufferInfo.offset = 0;
//     bufferInfo.range = allocationSize;
//
//     VkWriteDescriptorSet descriptorWrite{};
//     descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//     descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
//     descriptorWrite.dstBinding = bindingIndex;
//     descriptorWrite.dstArrayElement = 0;
//     descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//     descriptorWrite.descriptorCount = 1;
//     descriptorWrite.pBufferInfo = &bufferInfo;
//     descriptorWrite.pImageInfo = nullptr; // Optional
//     descriptorWrite.pTexelBufferView = nullptr; // Optional
//     vkUpdateDescriptorSets(Engine::GetContext().GetDevice()->GetVKDevice(),
//     1, &descriptorWrite, 0, nullptr);
// }
// UniformBuffer::~UniformBuffer()
//{
//     vkDestroyBuffer(Engine::GetContext().GetDevice()->GetVKDevice(),
//     m_Buffer, nullptr);
//     vkFreeMemory(Engine::GetContext().GetDevice()->GetVKDevice(),
//     m_BufferMemory, nullptr);
// }
// void UniformBuffer::UpdateUniformBuffer(void* dataToCopy, size_t dataSize)
//{
//     void* bufferHandle;
//     vkMapMemory(Engine::GetContext().GetDevice()->GetVKDevice(),
//     m_BufferMemory, 0, dataSize, 0, &bufferHandle); memcpy(bufferHandle,
//     dataToCopy, dataSize);
//     vkUnmapMemory(Engine::GetContext().GetDevice()->GetVKDevice(),
//     m_BufferMemory);
// }
