#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
#include "core.h"
#include "Containers.h"
#include "DescriptorSet.h"
namespace Anor
{
	class Texture;
	class DescriptorSet;
	class VertexBuffer
	{
	public:
		VertexBuffer(const std::vector<Vertex>& vertices);
		VertexBuffer(const float* vertices, size_t bufferSize);
		~VertexBuffer();
		const std::vector<Vertex>&	GetVertices() { return m_Vertices; }
		const VkBuffer&				GetVKBuffer() { return m_Buffer; }
	private:
		VkBuffer	   m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;
		std::vector<Vertex> m_Vertices;
	};

	class IndexBuffer 
	{
	public:
		IndexBuffer(const std::vector<uint32_t>& indices);
		~IndexBuffer();
		const std::vector<uint32_t>& GetIndices() { return m_Indices; }
		const VkBuffer&				 GetVKBuffer() { return m_Buffer; }
	private:
		VkBuffer	   m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;
		std::vector<uint32_t> m_Indices;
	};

	class UniformBuffer 
	{
	public:
		UniformBuffer(const Ref<DescriptorSet>& dscSet, size_t allocationSize, uint32_t bindingIndex);
		~UniformBuffer();
		const VkBuffer&		  GetUniformBuffer() { return m_Buffer; }
		const VkDeviceMemory& GetBufferMemory() { return m_BufferMemory; }
	private:
		VkBuffer	   m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;
	};
}