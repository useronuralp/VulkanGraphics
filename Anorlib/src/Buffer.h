#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
namespace Anor
{
	class LogicalDevice;
	class PhysicalDevice;
	class CommandPool;

	struct Vertex
	{
		glm::vec2 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
	};
	class Buffer
	{
	protected:
		Buffer(PhysicalDevice* pPhysicalDevice, LogicalDevice* pLogicalDevice, CommandPool* cmdPool) : m_PhysicalDevice(pPhysicalDevice), m_Device(pLogicalDevice), m_CommandPool(cmdPool){}
		Buffer(PhysicalDevice* pPhysicalDevice, LogicalDevice* pLogicalDevice) : m_PhysicalDevice(pPhysicalDevice), m_Device(pLogicalDevice) {}
		virtual ~Buffer() = 0;
	protected:
		void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		uint32_t FindMemoryType(uint32_t reqs, VkMemoryPropertyFlags flags);
	protected:
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_BufferMemory = VK_NULL_HANDLE;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		PhysicalDevice* m_PhysicalDevice = nullptr;
		LogicalDevice* m_Device = nullptr;
		CommandPool* m_CommandPool = nullptr;
	};
	class ImageBuffer : public Buffer
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			PhysicalDevice* pPhysicalDevice;
			CommandPool* pCommandPool;
			const char* pTexturePath;
			bool		IncludeAlpha = true;
			VkFormat	ImageFormat;
			VkImageTiling Tiling;
			VkImageUsageFlags Usage;
			VkMemoryPropertyFlags Properties;
			uint32_t	AnisotropyFilter;
			VkSamplerAddressMode TextureMode; // Can make this separate for each axis.
		};
	public:
		ImageBuffer(CreateInfo& createInfo);
		VkImageView& GetVKImageView() { return m_ImageView; }
		VkSampler& GetVKSampeler() { return m_Sampler; }
		void UpdateImageBuffer(VkDescriptorSet& dscSet);
		~ImageBuffer();
		void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height);
	private:
		VkImage m_Image				 = VK_NULL_HANDLE;
		VkDeviceMemory m_ImageMemory = VK_NULL_HANDLE;
		VkImageView m_ImageView		 = VK_NULL_HANDLE;
		VkSampler m_Sampler;


		const char* m_TexturePath;
		uint32_t	m_ImageWidth;
		uint32_t	m_ImageHeight;
		int			m_ChannelCount;
	};
	class VertexBuffer : public Buffer
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			PhysicalDevice* pPhysicalDevice;
			std::vector<Vertex>* pVertices;
			CommandPool* pCommandPool;
		};
	public:
		VertexBuffer(CreateInfo& createInfo);
		~VertexBuffer();
		std::vector<Vertex> GetVertices() { return *m_Vertices; }
		VkBuffer GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<Vertex>* m_Vertices;
	};

	class IndexBuffer : public Buffer
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			PhysicalDevice* pPhysicalDevice;
			std::vector<uint16_t>* pIndices;
			CommandPool* pCommandPool;
		};
	public:
		IndexBuffer(CreateInfo& createInfo);
		~IndexBuffer();
		std::vector<uint16_t> GetIndices() { return *m_Indices; }
		VkBuffer GetVKBuffer() { return m_Buffer; }
	private:
		std::vector<uint16_t>* m_Indices;
	};

	class UniformBuffer : public Buffer
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			PhysicalDevice* pPhysicalDevice;
			uint32_t		AllocateSize;
		};
	public:
		UniformBuffer(CreateInfo& createInfo);
		~UniformBuffer();
		VkBuffer GetUniformBuffer() { return m_Buffer; }
		VkDeviceMemory GetBufferMemory() { return m_BufferMemory; }
		void UpdateUniformBuffer(uint64_t writeRange, VkDescriptorSet& dscSet);
	};
}