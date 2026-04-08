#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

class DescriptorSet;
class VertexBuffer
{
   public:
    VertexBuffer(const std::vector<float>& vertices);
    VertexBuffer(const float* vertices, size_t bufferSize);
    ~VertexBuffer();
    const std::vector<float>& GetVertices()
    {
        return m_Vertices;
    }
    const VkBuffer& GetVKBuffer()
    {
        return m_Buffer;
    }

   private:
    VkBuffer           m_Buffer       = VK_NULL_HANDLE;
    VkDeviceMemory     m_BufferMemory = VK_NULL_HANDLE;
    std::vector<float> m_Vertices;
};

class IndexBuffer
{
   public:
    IndexBuffer(const std::vector<uint32_t>& indices);
    ~IndexBuffer();
    const std::vector<uint32_t>& GetIndices()
    {
        return m_Indices;
    }
    const VkBuffer& GetVKBuffer()
    {
        return m_Buffer;
    }

   private:
    VkBuffer              m_Buffer       = VK_NULL_HANDLE;
    VkDeviceMemory        m_BufferMemory = VK_NULL_HANDLE;
    std::vector<uint32_t> m_Indices;
};
