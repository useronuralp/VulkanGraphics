#include "Utils.h"
#include <iostream>
#include <fstream>
#include "core.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "CommandBuffer.h"
namespace OVK
{
	void Utils::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callbackFNC)
	{
		createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		createInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
		createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
		createInfo.pfnUserCallback = callbackFNC;
	}
	std::vector<char> Utils::ReadFile(const std::string& filePath)
    {
        // ate : Start reading at the end of the file
        // binary: read the file as binary, don't do char conversion.
        
        std::string path = std::string(SOLUTION_DIR) + "OVKLib\\" + filePath;
        std::ifstream file(path, std::ios::ate | std::ios::binary);

        //std::cout << std::filesystem::current_path() << std::endl;

        if (!file.is_open())
        {
            std::cerr << "Error:" << errno << std::endl;
            //throw std::runtime_error("Failed to open file!");
        }
        // tellg(): tellg() gives you the current position of the reader head. In this case we opened the file by starting to read if from the end so it gives us the size.
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize); // Reads the contents of "file" into buffer with the help of buffer.data() pointer. The size of the reading is "fileSize";

        file.close();

        return buffer;
    }

    void Utils::CreateVKBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
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

    uint32_t Utils::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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

    void Utils::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandPool singleCmdPool;
        VkCommandBuffer singleCmdBuffer;
        CommandBuffer::CreateCommandPool(VulkanApplication::s_TransferQueueFamily, singleCmdPool);
        CommandBuffer::CreateCommandBuffer(singleCmdBuffer, singleCmdPool);
        CommandBuffer::BeginRecording(singleCmdBuffer);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(singleCmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        CommandBuffer::EndRecording(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer, VulkanApplication::s_Device->GetTransferQueue()); // Graphics queue usually supports transfer operations as well. At least in NVIDIA cards.
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool, VulkanApplication::s_Device->GetTransferQueue()); // Wait for the queue to idle to free the cmd buffer.
        CommandBuffer::DestroyCommandPool(singleCmdPool);
    }

    VkSampler Utils::CreateSampler(Ref<Image> image, ImageType imageType, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkBool32 anisotrophy)
    {
        VkSampler sampler;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        if (imageType == ImageType::COLOR)
        {
            // Repeats the texture when going out of the sampling range. You might wanna expose this variable during ImageBufferCreation.
            samplerInfo.addressModeU = addressMode;
            samplerInfo.addressModeV = addressMode;
            samplerInfo.addressModeW = addressMode;
            samplerInfo.anisotropyEnable = anisotrophy;
            samplerInfo.maxAnisotropy = anisotrophy ? VulkanApplication::s_PhysicalDevice->GetVKProperties().limits.maxSamplerAnisotropy : 0;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = static_cast<float>(image->GetMipLevel());
            samplerInfo.magFilter = magFilter;
            samplerInfo.minFilter = minFilter;

            ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &sampler) == VK_SUCCESS, "Failed to create texture sampler!");
        }
        else // Depth
        {
            samplerInfo.magFilter = magFilter; 
            samplerInfo.minFilter = minFilter;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = addressMode;
            samplerInfo.addressModeV = addressMode;
            samplerInfo.addressModeW = addressMode;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.maxAnisotropy = 1.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 1.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

            ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &sampler) == VK_SUCCESS, "Failed to create texture sampler!");
        }

        return sampler;
    }

    VkSampler Utils::CreateCubemapSampler()
    {
        VkSampler sampler;
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
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &sampler) == VK_SUCCESS, "Failed to create texture sampler!");

        return sampler;
    }

    void Utils::WriteDescriptorSetWithSampler(const VkDescriptorSet& dscSet, const VkSampler& sampler, const VkImageView& imageView, uint32_t bindingIndex, VkImageLayout layout, int arrayIndex)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = layout;
        imageInfo.imageView = imageView;
        imageInfo.sampler = sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet;
        descriptorWrite.dstBinding = bindingIndex;
        descriptorWrite.dstArrayElement = arrayIndex;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }

    VkFormat Utils::FindDepthFormat()
    {
        return FindSupportedFormat({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    VkFormat Utils::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
            {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
            {
                return format;
            }
        }
        ASSERT(false, "Failed to find depth format");
    }
}