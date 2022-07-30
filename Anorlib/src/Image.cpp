#include "Image.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "CommandBuffer.h"

#include <stb_image.h>
namespace Anor
{
    // This constructor is to be used when you want to initialize a VkImage with as a color/texture buffer:
	Image::Image(const char* imagePathToLoad, VkFormat imageFormat)
        :m_Path(imagePathToLoad)
	{
        // Load the image here.
        int texWidth;
        int texHeight;
        int texChannels;

        stbi_uc* pixels = stbi_load(imagePathToLoad, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); // TO DO: Not every image must be an RGBA type. Might wanna use an if else check here.
        ASSERT(pixels, "Failed to load texture.");

        // Set member variables.
        m_Width = texWidth;
        m_Height = texHeight;
        m_ChannelCount = texChannels;
        m_ImageSize = texWidth * texHeight * 4;

        SetupImage(m_Width, m_Height, imageFormat);

        // Prep the staging buffer.
        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        Utils::CreateVKBuffer(m_ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Copy the Texture data to the staging buffer.
        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, m_ImageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(m_ImageSize));
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);

        // Do a layout transition operation to move the data from the staging buffer to the VkImage.
        // TO DO : Learn pipeline barriers.
        TransitionImageLayout(imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, m_Width, m_Height);
        TransitionImageLayout(imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);

        stbi_image_free(pixels);
	}

    Image::Image(uint32_t width, uint32_t height, VkFormat imageFormat)
    {
        SetupImage(width, height, imageFormat);
    }

	Image::~Image()
	{
        vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), m_ImageView, nullptr);
        vkDestroyImage(VulkanApplication::s_Device->GetVKDevice(), m_Image, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_ImageMemory, nullptr);
	}

    void Image::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
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

        vkCmdPipelineBarrier(singleCmdBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
    }

    void Image::CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height)
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

    void Image::SetupImage(uint32_t width, uint32_t height, VkFormat imageFormat)
    {
        // VK Image creation.
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.flags = 0; // Optional

        ASSERT(vkCreateImage(VulkanApplication::s_Device->GetVKDevice(), &imageCreateInfo, nullptr, &m_Image) == VK_SUCCESS, "Failed to create image!");

        // Mem allocation.
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(VulkanApplication::s_Device->GetVKDevice(), m_Image, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = Utils::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        ASSERT(vkAllocateMemory(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, nullptr, &m_ImageMemory) == VK_SUCCESS, "Failed to allocate image memory!");
        vkBindImageMemory(VulkanApplication::s_Device->GetVKDevice(), m_Image, m_ImageMemory, 0);

        // Create image view to access the texture.
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create texture image view!");
    }

}