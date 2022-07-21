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
	struct ImageBufferSpecs
	{
		Ref<Texture>		   Texture;
		Ref<DescriptorSet> DescriptorSet;
		uint32_t		   BindingIndex;
	};
	struct UniformBufferSpecs
	{
		size_t			   BufferSize;
		Ref<DescriptorSet> DescriptorSet;
		uint32_t		   BindingIndex;
	};
	class DescriptorSet;
	class Buffer
	{
	protected:
		Buffer() = default;
		virtual ~Buffer() = 0;
	protected:
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		uint32_t FindMemoryType(uint32_t reqs, VkMemoryPropertyFlags flags);
	protected:
		VkBuffer m_Buffer				= VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory	= VK_NULL_HANDLE;
	};
	class ImageBuffer : public Buffer
	{
	public:
		ImageBuffer(const ImageBufferSpecs& specs);
		const VkImageView& GetVKImageView() { return m_ImageView; }
		const VkSampler& GetVKSampeler() { return m_Sampler; }
		~ImageBuffer();
		void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height);
		std::string GetPath()& { return m_TexturePath; }
	private:
		VkImage			m_Image			= VK_NULL_HANDLE; // TO DO: Have a vector of images. However many tetures an object uses, store 'em here. And give them all a texture ID.
		VkDeviceMemory	m_ImageMemory	= VK_NULL_HANDLE;
		VkImageView		m_ImageView		= VK_NULL_HANDLE;
		VkSampler		m_Sampler;

		std::string m_TexturePath;
		uint32_t	m_ImageWidth;
		uint32_t	m_ImageHeight;
		int			m_ChannelCount;
	};
	class VertexBuffer : public Buffer
	{
	public:
		VertexBuffer(const std::vector<Vertex>& vertices);
		~VertexBuffer();
		const std::vector<Vertex>&	GetVertices() { return m_Vertices; }
		const VkBuffer&				GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<Vertex> m_Vertices;
	};

	class IndexBuffer : public Buffer
	{
	public:
		IndexBuffer(const std::vector<uint32_t>& indices);
		~IndexBuffer();
		const std::vector<uint32_t>& GetIndices() { return m_Indices; }
		const VkBuffer& GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<uint32_t> m_Indices;
	};

	class UniformBuffer : public Buffer
	{
	public:
		UniformBuffer(const UniformBufferSpecs& specs);
		~UniformBuffer();
		const VkBuffer& GetUniformBuffer() { return m_Buffer; }
		const VkDeviceMemory& GetBufferMemory() { return m_BufferMemory; }
	};
}