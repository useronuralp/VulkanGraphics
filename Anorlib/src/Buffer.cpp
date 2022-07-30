#include "Buffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include <iostream>
#include "PhysicalDevice.h"
#include <stb_image.h>
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Texture.h"
#include "Utils.h"

namespace Anor
{
    VertexBuffer::VertexBuffer(const std::vector<Vertex>& vertices)
        :m_Vertices(vertices)
    {
        VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
        // The buffer we create on host side.
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, &memRequirements);
        memcpy_s(data, memRequirements.size, m_Vertices.data(), (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped "data" pointer.
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);


        // The following buffer is not visible to CPU.
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

        Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize, VulkanApplication::s_GraphicsQueueIndex);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }

    VertexBuffer::VertexBuffer(const float* vertices, size_t bufferSize)
    {
        // The buffer we create on host side.
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, &memRequirements);
        memcpy_s(data, memRequirements.size, vertices, (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped "data" pointer.
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);


        // The following buffer is not visible to CPU.
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

        Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize, VulkanApplication::s_GraphicsQueueIndex);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }

    VertexBuffer::~VertexBuffer()
    {
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }

    IndexBuffer::IndexBuffer(const std::vector<uint32_t>& indices)
        :m_Indices(indices)
    {
        VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_Indices.data(), (size_t)bufferSize);
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);

        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

        Utils::CopyBuffer(stagingBuffer, m_Buffer, bufferSize, VulkanApplication::s_GraphicsQueueIndex);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }
    IndexBuffer::~IndexBuffer()
    {
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }
    UniformBuffer::UniformBuffer(const Ref<DescriptorSet>& dscSet, size_t allocationSize, uint32_t bindingIndex)
    {
        VkDeviceSize bufferSize = allocationSize;
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Buffer, m_BufferMemory);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_Buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = allocationSize;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = bindingIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    UniformBuffer::~UniformBuffer()
    {
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }
}
