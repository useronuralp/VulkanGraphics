#include "Image.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "CommandBuffer.h"
#include "PhysicalDevice.h"
#include <stb_image.h>
namespace OVK
{

    // This constructor is to be used when you want to initialize a VkImage with as a color/texture buffer:
	Image::Image(std::vector<std::string> textures, VkFormat imageFormat) : m_ImageFormat(imageFormat)
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

        // Texture data loading.
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

        if (m_IsCubemap)
        {
            m_MipLevels = 1;
        }
        else
        {
            m_MipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
        }

        SetupImage(m_Width, m_Height, m_ImageFormat, (VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), ImageType::COLOR);

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

        // Transition the layout to allow copying to m_Image.
        TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        // Copy the data to m_Image.
        CopyBufferToImage(stagingBuffer, m_Width, m_Height);
        // If the image is a cubemap we are done and we can transition the layout to the final format to make it readable from shaders.
        if (m_IsCubemap)
        {
            TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), stagingBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), stagingBufferMemory, nullptr);

        // If the image is not a cubemap we want to generate the mipmaps. This step will automatically transtion the image to a shader readable format when it is done.
        if(!m_IsCubemap)
            GenerateMipmaps();

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

    Image::Image(uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usageFlags, ImageType imageType) : m_ImageFormat(imageFormat)
    {
        m_Width = width;
        m_Height = height;
        SetupImage(width, height, m_ImageFormat, usageFlags, imageType);
        m_MipLevels = 1;
    }

	Image::~Image()
	{
        vkDestroyImageView(VulkanApplication::s_Device->GetVKDevice(), m_ImageView, nullptr);
        vkDestroyImage(VulkanApplication::s_Device->GetVKDevice(), m_Image, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), m_ImageMemory, nullptr);
	}

    void Image::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        uint32_t layerCount = 1;
        if (m_IsCubemap)
        {
            layerCount = 6;
        }
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::CreateCommandBufferPool(VulkanApplication::s_GraphicsQueueFamily, singleCmdPool);
        CommandBuffer::CreateCommandBuffer(singleCmdBuffer, singleCmdPool);
        CommandBuffer::BeginRecording(singleCmdBuffer);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_Image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = m_MipLevels;
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
        CommandBuffer::EndRecording(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer, VulkanApplication::s_Device->GetGraphicsQueue());
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool, VulkanApplication::s_Device->GetGraphicsQueue());
        CommandBuffer::DestroyCommandPool(singleCmdPool);
    }

    void Image::CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height)
    {
        VkCommandBuffer singleCmdBuffer;
        VkCommandPool singleCmdPool;
        CommandBuffer::CreateCommandBufferPool(VulkanApplication::s_TransferQueueFamily, singleCmdPool);
        CommandBuffer::CreateCommandBuffer(singleCmdBuffer, singleCmdPool);
        CommandBuffer::BeginRecording(singleCmdBuffer);

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

        CommandBuffer::EndRecording(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer, VulkanApplication::s_Device->GetTransferQueue());
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool, VulkanApplication::s_Device->GetTransferQueue());
        CommandBuffer::DestroyCommandPool(singleCmdPool);
    }

    void Image::SetupImage(uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usageFlags, ImageType imageType)
    {
        if (imageType == ImageType::DEPTH_CUBEMAP)
        {
            m_IsCubemap = true;
        }

        uint32_t layerCount = 1;
        uint32_t arrayLayers = 1;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkImageCreateFlags flags = 0;

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
        imageCreateInfo.mipLevels = m_MipLevels;
        imageCreateInfo.arrayLayers = arrayLayers; 
        imageCreateInfo.format = imageFormat;
        imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = usageFlags;
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
        viewInfo.subresourceRange.levelCount = m_MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = layerCount;

        ASSERT(vkCreateImageView(VulkanApplication::s_Device->GetVKDevice(), &viewInfo, nullptr, &m_ImageView) == VK_SUCCESS, "Failed to create texture image view!");
    }

    void Image::GenerateMipmaps()
    {
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), m_ImageFormat, &formatProperties);

        ASSERT(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, "Texture image format does not support linear blitting!");

        VkCommandBuffer cmdBuffer;
        VkCommandPool cmdPool;
        CommandBuffer::CreateCommandBufferPool(VulkanApplication::s_TransferQueueFamily, cmdPool);
        CommandBuffer::CreateCommandBuffer(cmdBuffer, cmdPool);
        CommandBuffer::BeginRecording(cmdBuffer);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = m_Image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = m_Width;
        int32_t mipHeight = m_Height;

        for (uint32_t i = 1; i < m_MipLevels; i++)
        {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(cmdBuffer,
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                m_Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(cmdBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;


        }

        barrier.subresourceRange.baseMipLevel = m_MipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(cmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        CommandBuffer::EndRecording(cmdBuffer);
        CommandBuffer::Submit(cmdBuffer, VulkanApplication::s_Device->GetTransferQueue());
        CommandBuffer::FreeCommandBuffer(cmdBuffer, cmdPool, VulkanApplication::s_Device->GetTransferQueue());
        CommandBuffer::DestroyCommandPool(cmdPool);
    }

}