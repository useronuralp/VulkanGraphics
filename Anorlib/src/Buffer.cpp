#include "Buffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include <iostream>
#include "PhysicalDevice.h"
#include <stb_image.h>
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Texture.h"

namespace Anor
{
	 VertexBuffer::VertexBuffer(const std::vector<Vertex>& vertices)
        :Buffer(), m_Vertices(vertices)
	{
         VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices.size();
         // The buffer we create on host side.
         VkBuffer stagingBuffer;
         VkDeviceMemory stagingBufferMemory;

         CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

         void* data;
         vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);

         VkMemoryRequirements memRequirements;
         vkGetBufferMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, &memRequirements);
         memcpy_s(data, memRequirements.size, m_Vertices.data(), (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped "data" pointer.
         vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);


         // The following buffer is not visible to CPU.
         CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

         CopyBuffer(stagingBuffer, m_Buffer, bufferSize, VulkanApplication::s_GraphicsQueueIndex);

         vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
         vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);
	}
  
    VertexBuffer::~VertexBuffer()
    {
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }

    IndexBuffer::IndexBuffer(const std::vector<uint32_t>& indices)
        :Buffer(), m_Indices(indices)
    {
        VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices.size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_Indices.data(), (size_t)bufferSize);
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);
        
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);
        
        CopyBuffer(stagingBuffer, m_Buffer, bufferSize, VulkanApplication::s_GraphicsQueueIndex);
        
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }
    IndexBuffer::~IndexBuffer()
    {
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }
    UniformBuffer::UniformBuffer(const UniformBufferSpecs& specs)
        :Buffer()
    {
        VkDeviceSize bufferSize = specs.BufferSize;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Buffer, m_BufferMemory);

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_Buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = specs.BufferSize;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = specs.DescriptorSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = specs.BindingIndex;
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
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice() , m_BufferMemory, nullptr);
    }
    
    void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex)
    {
        VkCommandPool singleCmdPool;
        VkCommandBuffer singleCmdBuffer;
        CommandBuffer::Create(graphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(singleCmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
        // This line also takes care of submitting the command buffer and making sure it runs before freeing it.
        //CommandBuffer::EndSingleTimeCommandBuffer(m_Device, singleCmdBuffer, singleCmdPool);
    }
    void Buffer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        ASSERT(vkCreateBuffer(VulkanApplication::s_Device->GetVKDevice(), &bufferInfo, nullptr, &buffer) == VK_SUCCESS, "Failed to create vertex buffer");

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        ASSERT(vkAllocateMemory(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, nullptr, &bufferMemory) == VK_SUCCESS, "Failed to allocate vertex buffer memory!");
        vkBindBufferMemory(VulkanApplication::s_Device->GetVKDevice(), buffer, bufferMemory, 0);
    }
    uint32_t Buffer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties = VulkanApplication::s_PhysicalDevice->GetVKDeviceMemoryProperties();
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

    ImageBuffer::ImageBuffer(const ImageBufferSpecs& specs)
       :Buffer(), m_TexturePath(specs.Texture->GetPath())
    {
        //int texWidth, texHeight, texChannels;
        
        stbi_uc* pixels = specs.Texture->GetPixels();
        VkDeviceSize imageSize = specs.Texture->GetImageSize();
        m_ImageWidth    = specs.Texture->GetWidth();
        m_ImageHeight   = specs.Texture->GetHeight();
        m_ChannelCount  = specs.Texture->GetChannels();
        
        //ASSERT(pixels, "Failed to load texture image!");

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);


        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);

        // Staging buffer contains pixel values at this instant.


        // VK Image creation.
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = static_cast<uint32_t>(m_ImageWidth);
        imageCreateInfo.extent.height = static_cast<uint32_t>(m_ImageHeight);
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.flags = 0; // Optional

        ASSERT(vkCreateImage(VulkanApplication::s_Device->GetVKDevice(), &imageCreateInfo, nullptr, &m_Image) == VK_SUCCESS, "Failed to create image!");

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), m_Image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        ASSERT(vkAllocateMemory(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, nullptr, &m_ImageMemory) == VK_SUCCESS, "Failed to allocate image memory!");
        vkBindImageMemory(VulkanApplication::s_Device->GetVKDevice(), m_Image, m_ImageMemory, 0);

        // TO DO : Learn pipeline barriers.
        TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, static_cast<uint32_t>(m_ImageWidth), static_cast<uint32_t>(m_ImageHeight));
        TransitionImageLayout(VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);

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

        ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create texture image view!");

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        // Repeats the texture when going out of the sampling range. You might wanna expose this variable during ImageBufferCreation.
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = VulkanApplication::s_PhysicalDevice->GetVKProperties().limits.maxSamplerAnisotropy;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &m_Sampler) == VK_SUCCESS, "Failed to create texture sampler!");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_ImageView;
        imageInfo.sampler = m_Sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = specs.DescriptorSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = specs.BindingIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    ImageBuffer::~ImageBuffer()
    {
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), m_Sampler, nullptr);
        vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), m_ImageView, nullptr);
        vkDestroyImage(VulkanApplication::s_Device->GetVKDevice(), m_Image, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_ImageMemory, nullptr);
    }
    void ImageBuffer::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(VulkanApplication::s_GraphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

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
        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
    }
    void ImageBuffer::CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(VulkanApplication::s_GraphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

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
        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
    }
}
