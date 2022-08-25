#include "Image.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "CommandBuffer.h"

#include <stb_image.h>
namespace Anor
{

    // This constructor is to be used when you want to initialize a VkImage with as a color/texture buffer:
	Image::Image(std::vector<std::string> textures, VkFormat imageFormat)
	{
        stbi_uc* pixels = nullptr;
        stbi_uc* cubemapTextures[6];
        VkDeviceSize imageSize;
        VkDeviceSize layerSize;

        if (textures.size() == 6)
        {
            m_IsCubemap = true;
            m_CubemapTexPaths = textures;
        }
        else
        {
            m_Path = textures[0];
        }

        int texWidth;
        int texHeight;
        int texChannels;

        if (m_IsCubemap)
        {
            for (uint32_t i = 0; i < 6; i++)
            {
                cubemapTextures[i] = stbi_load(textures[i].c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                ASSERT(cubemapTextures[i], "Failed to load texture.");
            }
            imageSize = texWidth * texHeight * 4 * 6;
            layerSize = imageSize / 6;
        }
        else
        {
            pixels = stbi_load(m_Path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha); 
            ASSERT(pixels, "Failed to load texture.");
            imageSize = texWidth * texHeight * 4;
            layerSize = imageSize;
        }


        // Set member variables.
        m_Width = texWidth;
        m_Height = texHeight;
        m_ChannelCount = texChannels;
        m_ImageSize = imageSize;
        m_LayerSize = layerSize;

        m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

        SetupImage(m_Width, m_Height, imageFormat);

        // Prep the staging buffer.
        VkBuffer       stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        Utils::CreateVKBuffer(m_ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

        // Copy the Texture data to the staging buffer.
        void* data;
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, 0, m_ImageSize, 0, &data);
        if (m_IsCubemap)
        {
            for (uint32_t i = 0; i < 6; i++)
            {
                memcpy(static_cast<stbi_uc*>(data) + (m_LayerSize * i), cubemapTextures[i], m_LayerSize);
            }
        }
        else
        {
            memcpy(data, pixels, static_cast<size_t>(m_ImageSize));
        }
        vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory);

        // Do a layout transition operation to move the data from the staging buffer to the VkImage.
        // TO DO : Learn pipeline barriers.
        TransitionImageLayout(imageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, m_Width, m_Height);
        TransitionImageLayout(imageFormat, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);

        if (m_IsCubemap)
        {
            for (int i = 0; i < 6; i++)
            {
                stbi_image_free(cubemapTextures[i]);
            }
        }
        else
        {
            stbi_image_free(pixels);
        }
	}

    Image::Image(uint32_t width, uint32_t height, VkFormat imageFormat, ImageType imageType)
    {
        m_Width = width;
        m_Height = height;
        SetupImage(width, height, imageFormat, imageType);
    }

	Image::~Image()
	{
        vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), m_ImageView, nullptr);
        vkDestroyImage(VulkanApplication::s_Device->GetVKDevice(), m_Image, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_ImageMemory, nullptr);
	}

    void Image::TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        uint32_t layerCount = 1;
        if (m_IsCubemap)
        {
            layerCount = 6;
        }
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(VulkanApplication::s_GraphicsANDComputeQueueIndex, singleCmdPool, singleCmdBuffer);
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
        barrier.subresourceRange.layerCount = layerCount;

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

    void Image::TransitionDepthLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::Create(VulkanApplication::s_GraphicsANDComputeQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        bool supported = false;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
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
        CommandBuffer::Create(VulkanApplication::s_GraphicsANDComputeQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

        VkBufferImageCopy region{};
        std::vector<VkBufferImageCopy> bufferCopyRegions;

        if (m_IsCubemap)
        {
            for (uint32_t i = 0; i < 6; i++)
            {
                for (uint32_t mipLevel = 0; mipLevel < 1; mipLevel++)
                {

                    VkBufferImageCopy region{};
                    region.bufferOffset = m_LayerSize * i;
                    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    region.imageSubresource.mipLevel = mipLevel;
                    region.imageSubresource.baseArrayLayer = i;
                    region.imageSubresource.layerCount = 1;
                    region.imageExtent.width = width >> mipLevel;
                    region.imageExtent.height = height >> mipLevel;
                    region.imageExtent.depth = 1;

                    bufferCopyRegions.push_back(region);
                }
            }
        }
        else
        {
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;

            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;

            region.imageOffset = { 0, 0, 0 };
            region.imageExtent = { width, height, 1 };
        }
        
        if (m_IsCubemap)
        {
            vkCmdCopyBufferToImage(singleCmdBuffer, buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, bufferCopyRegions.size(), bufferCopyRegions.data());
        }
        else
        {
            vkCmdCopyBufferToImage( singleCmdBuffer, buffer, m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
    }

    void Image::SetupImage(uint32_t width, uint32_t height, VkFormat imageFormat, ImageType imageType)
    {
        VkImageCreateFlags flags = 0;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        uint32_t arrayLayers = 1;
        uint32_t layerCount = 1;

        if (m_IsCubemap)
        {
            layerCount = 6;
            arrayLayers = 6;
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        }

        // VK Image creation.
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = width;
        imageCreateInfo.extent.height = height;
        imageCreateInfo.extent.depth = 1;
        imageCreateInfo.mipLevels = 1;
        imageCreateInfo.arrayLayers = arrayLayers;
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = imageType == ImageType::COLOR ? (VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) : (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.flags = flags;

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
        viewInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
        viewInfo.viewType = viewType;
        viewInfo.format = imageFormat;
        viewInfo.subresourceRange.aspectMask = imageType == ImageType::COLOR ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT; // This part also changes when shadowmapping.
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = layerCount;

        ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create texture image view!");
    }

}