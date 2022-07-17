#include "Buffer.h"
#include "LogicalDevice.h"
#include <iostream>
#include "PhysicalDevice.h"
#include <stb_image.h>
#include "CommandBuffer.h"
#include "DescriptorSet.h"

namespace Anor
{
	 VertexBuffer::VertexBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, const std::vector<Vertex>& vertices, uint64_t graphicsQueueIndex)
        :Buffer(physicalDevice, device, graphicsQueueIndex), m_Vertices(vertices)
	{
         VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
         // The buffer we create on host side.
         VkBuffer stagingBuffer;
         VkDeviceMemory stagingBufferMemory;

         CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

         void* data;
         vkMapMemory(m_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

         VkMemoryRequirements memRequirements;
         vkGetBufferMemoryRequirements(m_Device->GetVKDevice(), stagingBuffer, &memRequirements);
         memcpy_s(data, memRequirements.size, m_Vertices.data(), (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped "data" pointer.
         vkUnmapMemory(m_Device->GetVKDevice(), stagingBufferMemory);


         // The following buffer is not visible to CPU.
         CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

         CopyBuffer(stagingBuffer, m_Buffer, bufferSize, graphicsQueueIndex);

         vkDestroyBuffer(m_Device->GetVKDevice(), stagingBuffer, nullptr);
         vkFreeMemory(m_Device->GetVKDevice(), stagingBufferMemory, nullptr);
	}
  
    VertexBuffer::~VertexBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }

    IndexBuffer::IndexBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, const std::vector<uint32_t>& indices, uint64_t graphicsQueueIndex)
        :Buffer(physicalDevice, device, graphicsQueueIndex), m_Indices(indices)
    {
        VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(m_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_Indices.data(), (size_t)bufferSize);
        vkUnmapMemory(m_Device->GetVKDevice(), stagingBufferMemory);
        
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);
        
        CopyBuffer(stagingBuffer, m_Buffer, bufferSize, m_GraphicsQueueIndex);
        
        vkDestroyBuffer(m_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }
    IndexBuffer::~IndexBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }
    UniformBuffer::UniformBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physicalDevice, size_t allocateSize)
        :Buffer(physicalDevice, device)
    {
        VkDeviceSize bufferSize = allocateSize;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Buffer, m_BufferMemory);
    }
    UniformBuffer::~UniformBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice() , m_BufferMemory, nullptr);
    }
    void ImageBuffer::UpdateImageBuffer(const Ref<DescriptorSet>& dscSet)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_ImageView;
        imageInfo.sampler = m_Sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    void UniformBuffer::UpdateUniformBuffer(uint64_t writeRange, const Ref<DescriptorSet>& dscSet)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_Buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = writeRange;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        vkUpdateDescriptorSets(m_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    
    void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex)
    {
        VkCommandPool singleCmdPool;
        VkCommandBuffer singleCmdBuffer;
        CommandBuffer::Create(m_Device, graphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::BeginSingleTimeCommandBuffer(singleCmdBuffer);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(singleCmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        // This line also takes care of submitting the command buffer and making sure it runs before freeing it.
        CommandBuffer::EndSingleTimeCommandBuffer(m_Device, singleCmdBuffer, singleCmdPool);
    }
    void Buffer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        ASSERT(vkCreateBuffer(m_Device->GetVKDevice(), &bufferInfo, nullptr, &buffer) == VK_SUCCESS, "Failed to create vertex buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device->GetVKDevice(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        ASSERT(vkAllocateMemory(m_Device->GetVKDevice(), &allocInfo, nullptr, &bufferMemory) == VK_SUCCESS, "Failed to allocate vertex buffer memory!");
        vkBindBufferMemory(m_Device->GetVKDevice(), buffer, bufferMemory, 0);
    }
    uint32_t Buffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties = m_PhysicalDevice->GetVKDeviceMemoryProperties();
        uint32_t memoryTypeIndex = -1;


        // TO DO: Understand this part and the bit shift.
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                memoryTypeIndex = i;
            }
        }

        ASSERT(memoryTypeIndex != -1, "Failed to find suitable memory type!");
        return memoryTypeIndex;
    }
    Buffer::~Buffer() {}

    ImageBuffer::ImageBuffer(const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physDevice, const char* texturePath, uint64_t graphicsQueueIndex)
       :Buffer(physDevice, device, graphicsQueueIndex), m_TexturePath(texturePath)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(m_TexturePath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // TO DO: Test this alpha part, might be problematic.
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        m_ImageWidth    = texWidth;
        m_ImageHeight   = texHeight;
        m_ChannelCount  = texChannels;

        ASSERT(pixels, "Failed to load texture image!");

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);


        void* data;
        vkMapMemory(m_Device->GetVKDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(m_Device->GetVKDevice(), stagingBufferMemory);

        stbi_image_free(pixels);

        // VK Image creation.
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = static_cast<uint32_t>(m_ImageWidth);
        imageInfo.extent.height = static_cast<uint32_t>(m_ImageHeight);
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0; // Optional

        ASSERT(vkCreateImage(m_Device->GetVKDevice(), &imageInfo, nullptr, &m_Image) == VK_SUCCESS, "Failed to create image!");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_Device->GetVKDevice(), m_Image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        ASSERT(vkAllocateMemory(m_Device->GetVKDevice(), &allocInfo, nullptr, &m_ImageMemory) == VK_SUCCESS, "Failed to allocate image memory!");
        vkBindImageMemory(m_Device->GetVKDevice(), m_Image, m_ImageMemory, 0);

        // TO DO : Learn pipeline barriers.
        TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


        vkDestroyBuffer(m_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), stagingBufferMemory, nullptr);

        // Create image view to access the texture.
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        ASSERT(vkCreateImageView(m_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create texture image view!");

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        // Repeats the texture when going out of the sampling range. You might wanna expose this variable during ImageBufferCreation.
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_PhysicalDevice->GetVKProperties().limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        ASSERT(vkCreateSampler(m_Device->GetVKDevice(), &samplerInfo, nullptr, &m_Sampler) == VK_SUCCESS, "Failed to create texture sampler!");
    }
    ImageBuffer::~ImageBuffer()
    {
        vkDestroySampler(m_Device->GetVKDevice(), m_Sampler, nullptr);
        vkDestroyImageView(m_Device->GetVKDevice(), m_ImageView, nullptr);
        vkDestroyImage(m_Device->GetVKDevice(), m_Image, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), m_ImageMemory, nullptr);
    }
    void ImageBuffer::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(m_Device, m_GraphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::BeginSingleTimeCommandBuffer(singleCmdBuffer);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        bool supported = false;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            supported = true;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            supported = true;
        }
        ASSERT(supported, "Unsupported layout transition");

        vkCmdPipelineBarrier(singleCmdBuffer, sourceStage , destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        CommandBuffer::EndSingleTimeCommandBuffer(m_Device, singleCmdBuffer, singleCmdPool);
    }
    void ImageBuffer::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(m_Device, m_GraphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::BeginSingleTimeCommandBuffer(singleCmdBuffer);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = {
            width,
            height,
            1
        };

        vkCmdCopyBufferToImage(
            singleCmdBuffer,
            buffer,
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
        CommandBuffer::EndSingleTimeCommandBuffer(m_Device, singleCmdBuffer, singleCmdPool);
    }
}
