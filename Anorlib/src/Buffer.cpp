#include "Buffer.h"
#include "LogicalDevice.h"
#include <iostream>
#include "PhysicalDevice.h"
#include "CommandPool.h"
#include <stb_image.h>
#include "CommandBuffer.h"
namespace Anor
{
	 VertexBuffer::VertexBuffer(CreateInfo& createInfo)
        :Buffer(createInfo.pPhysicalDevice, createInfo.pLogicalDevice, createInfo.pCommandPool), m_Vertices(createInfo.pVertices)
	{
        VkDeviceSize bufferSize = sizeof(m_Vertices[0]) * m_Vertices->size();
        // The buffer we create on host side.
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;

        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(m_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device->GetVKDevice(), stagingBuffer, &memRequirements);
        memcpy_s(data, memRequirements.size, m_Vertices->data(), (size_t)bufferSize); // Copy the vertex data to the GPU using the mapped "data" pointer.
        vkUnmapMemory(m_Device->GetVKDevice(), stagingBufferMemory);


        // The following buffer is not visible to CPU.
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);

        CopyBuffer(stagingBuffer, m_Buffer, bufferSize);

        vkDestroyBuffer(m_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), stagingBufferMemory, nullptr);
	}
  
    VertexBuffer::~VertexBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }

    IndexBuffer::IndexBuffer(CreateInfo& createInfo)
        :Buffer(createInfo.pPhysicalDevice, createInfo.pLogicalDevice, createInfo.pCommandPool), m_Indices(createInfo.pIndices)
    {
        VkDeviceSize bufferSize = sizeof(m_Indices[0]) * m_Indices->size();
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);
        
        void* data;
        vkMapMemory(m_Device->GetVKDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_Indices->data(), (size_t)bufferSize);
        vkUnmapMemory(m_Device->GetVKDevice(), stagingBufferMemory);
        
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_Buffer, m_BufferMemory);
        
        CopyBuffer(stagingBuffer, m_Buffer, bufferSize);
        
        vkDestroyBuffer(m_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), stagingBufferMemory, nullptr);
    }
    IndexBuffer::~IndexBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice(), m_BufferMemory, nullptr);
    }



    VkVertexInputBindingDescription Vertex::getBindingDescription()
    {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;;
    }

    std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};
        // For position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        // For the color
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        // For the texCoords
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
    UniformBuffer::UniformBuffer(CreateInfo& createInfo)
        :Buffer(createInfo.pPhysicalDevice, createInfo.pLogicalDevice)
    {
        VkDeviceSize bufferSize = createInfo.AllocateSize;
        CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_Buffer, m_BufferMemory);
    }
    UniformBuffer::~UniformBuffer()
    {
        vkDestroyBuffer(m_Device->GetVKDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Device->GetVKDevice() , m_BufferMemory, nullptr);
    }
    void ImageBuffer::UpdateImageBuffer(VkDescriptorSet& dscSet)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_ImageView;
        imageInfo.sampler = m_Sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet;
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(m_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    void UniformBuffer::UpdateUniformBuffer(uint64_t writeRange, VkDescriptorSet& dscSet)
    {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_Buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = writeRange;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional
        vkUpdateDescriptorSets(m_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    
    void Buffer::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool->GetCommandPool();
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_Device->GetVKDevice(), &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_Device->GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_Device->GetGraphicsQueue());

        vkFreeCommandBuffers(m_Device->GetVKDevice(), m_CommandPool->GetCommandPool(), 1, &commandBuffer);
    }
    void Buffer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Device->GetVKDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            std::cerr << "Failed to create vertex buffer" << std::endl;
            __debugbreak();
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Device->GetVKDevice(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_Device->GetVKDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            std::cerr << "failed to allocate vertex buffer memory!" << std::endl;
            __debugbreak();
        }

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

        if (memoryTypeIndex == -1)
        {
            std::cerr << "Failed to find suitable memory type!" << std::endl;
            __debugbreak();
        }

        return memoryTypeIndex;
    }
    Buffer::~Buffer() {}

    ImageBuffer::ImageBuffer(CreateInfo& createInfo)
       :Buffer(createInfo.pPhysicalDevice, createInfo.pLogicalDevice, createInfo.pCommandPool), m_TexturePath(createInfo.pTexturePath)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(m_TexturePath, &texWidth, &texHeight, &texChannels, createInfo.IncludeAlpha ? STBI_rgb_alpha : STBI_rgb); // TO DO: Test this alpha part, might be problematic.
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        m_ImageWidth    = texWidth;
        m_ImageHeight   = texHeight;
        m_ChannelCount  = texChannels;

        if (!pixels) {
            std::cerr << "Failed to load texture image!" << std::endl;
            __debugbreak();
        }

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
        imageInfo.format = createInfo.ImageFormat;
        imageInfo.tiling = createInfo.Tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = createInfo.Usage;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags = 0; // Optional

        if (vkCreateImage(m_Device->GetVKDevice(), &imageInfo, nullptr, &m_Image) != VK_SUCCESS)
        {
            std::cerr << "Failed to create image!" << std::endl;
            __debugbreak();
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(m_Device->GetVKDevice(), m_Image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_Device->GetVKDevice(), &allocInfo, nullptr, &m_ImageMemory) != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate image memory!" << std::endl;
            __debugbreak();
        }

        vkBindImageMemory(m_Device->GetVKDevice(), m_Image, m_ImageMemory, 0);

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

        if (vkCreateImageView(m_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
        {
            std::cerr << "Failed to create texture image view!" << std::endl;
            __debugbreak();
        }

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        // Repeats the texture when going out of the sampling range. You might wanna expose this variable during ImageBufferCreation.
        samplerInfo.addressModeU = createInfo.TextureMode;
        samplerInfo.addressModeV = createInfo.TextureMode;
        samplerInfo.addressModeW = createInfo.TextureMode;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = createInfo.AnisotropyFilter;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        if (vkCreateSampler(m_Device->GetVKDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
        {
            std::cerr << "Failed to create texture sampler!" << std::endl;
            __debugbreak();
        }
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
        CommandBuffer cmdBuffer(m_CommandPool, m_Device); // Create a single time command buffer here. The life of this object ends with this function.
        cmdBuffer.BeginSingleTimeCommands();

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

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            std::cerr << "Unsupported layout transition" << std::endl;
            __debugbreak();
        }

        vkCmdPipelineBarrier(cmdBuffer.GetVKCommandBuffer(), sourceStage , destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        cmdBuffer.EndnSingleTimeCommands();
    }
    void ImageBuffer::CopyBufferToImage(VkBuffer buffer, uint32_t width, uint32_t height)
    {
        CommandBuffer cmdBuffer(m_CommandPool, m_Device); 
        cmdBuffer.BeginSingleTimeCommands();

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
            cmdBuffer.GetVKCommandBuffer(),
            buffer,
            m_Image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        cmdBuffer.EndnSingleTimeCommands();
    }
}
