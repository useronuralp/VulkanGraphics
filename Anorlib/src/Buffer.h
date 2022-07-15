#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
#include "core.h"
namespace Anor
{
	class LogicalDevice;
	class PhysicalDevice;
	class DescriptorSet;
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
	};
	class Buffer
	{
	protected:
		Buffer(const Ref<PhysicalDevice>& physicalDevice, const Ref<LogicalDevice>& device, uint32_t graphicsQueueIndex = -1) : m_PhysicalDevice(physicalDevice), m_Device(device), m_GraphicsQueueIndex(graphicsQueueIndex) {}
		virtual ~Buffer() = 0;
	protected:
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		uint32_t FindMemoryType(uint32_t reqs, VkMemoryPropertyFlags flags);
	protected:
		VkBuffer m_Buffer				= VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory	= VK_NULL_HANDLE;
		uint32_t m_GraphicsQueueIndex = -1;
		Ref<PhysicalDevice> m_PhysicalDevice = nullptr;
		Ref<LogicalDevice> m_Device			 = nullptr;
	};
	class ImageBuffer : public Buffer
	{
	public:
		ImageBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physDevice, const char* texturePath, uint64_t graphicsQueueIndex);
		const VkImageView& GetVKImageView() { return m_ImageView; }
		const VkSampler& GetVKSampeler() { return m_Sampler; }
		void UpdateImageBuffer(const Ref<DescriptorSet> & dscSet);
		~ImageBuffer();
		void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);
	private:
		VkImage			m_Image			= VK_NULL_HANDLE;
		VkDeviceMemory	m_ImageMemory	= VK_NULL_HANDLE;
		VkImageView		m_ImageView		= VK_NULL_HANDLE;
		VkSampler		m_Sampler;

		const char* m_TexturePath;
		uint32_t	m_ImageWidth;
		uint32_t	m_ImageHeight;
		int			m_ChannelCount;
	};
	class VertexBuffer : public Buffer
	{
	public:
		VertexBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, const std::vector<Vertex>& vertices, uint64_t graphicsQueueIndex);
		~VertexBuffer();
		const std::vector<Vertex>&	GetVertices() { return m_Vertices; }
		const VkBuffer&				GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<Vertex> m_Vertices;
	};

	class IndexBuffer : public Buffer
	{
	public:
		IndexBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, const std::vector<uint32_t>& indices, uint64_t graphicsQueueIndex);
		~IndexBuffer();
		const std::vector<uint32_t>& GetIndices() { return m_Indices; }
		const VkBuffer& GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<uint32_t> m_Indices;
	};

	class UniformBuffer : public Buffer
	{
	public:
		UniformBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, size_t allocateSize);
		~UniformBuffer();
		const VkBuffer& GetUniformBuffer() { return m_Buffer; }
		const VkDeviceMemory& GetBufferMemory() { return m_BufferMemory; }
		void UpdateUniformBuffer(uint64_t writeRange, const Ref<DescriptorSet>& dscSet);
	};
}