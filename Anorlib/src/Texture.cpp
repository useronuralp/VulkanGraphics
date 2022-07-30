#include "Texture.h"
#include <iostream>
#include "core.h"
#include <stb_image.h>
#include "VulkanApplication.h"
#include "PhysicalDevice.h"
#include "LogicalDevice.h"
#include "DescriptorSet.h"
namespace Anor
{
	Texture::Texture(const char* path, VkFormat imageFormat)
	{
        m_Image = std::make_shared<Image>(path, imageFormat);
	}
	VkSampler Texture::CreateSamplerFromThisTexture(const Ref<DescriptorSet>& dscSet, uint32_t bindingIndex)
	{
        VkSampler samplerToReturn;

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

        ASSERT(vkCreateSampler(VulkanApplication::s_Device->GetVKDevice(), &samplerInfo, nullptr, &samplerToReturn) == VK_SUCCESS, "Failed to create texture sampler!");


        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = m_Image->GetImageView();
        imageInfo.sampler = samplerToReturn;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = bindingIndex;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);


        return samplerToReturn;
	}
}