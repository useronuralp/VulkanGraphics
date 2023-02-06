#pragma once
#include "vulkan/vulkan.h"
#include "Image.h"
#include <vector>
#include <string>
namespace OVK
{
	struct Utils
	{
		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callbackFNC);
		static std::vector<char> ReadFile(const std::string& filePath);
		static void CreateVKBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
		static VkSampler CreateSampler(Ref<Image> image, ImageType imageType, VkFilter magFilter, VkFilter minFilter, VkSamplerAddressMode addressMode, VkBool32 anisotrophy);
		static VkSampler CreateCubemapSampler();
		static VkFormat FindDepthFormat();
		static VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
		static void UpdateDescriptorSet(const VkDescriptorSet& dscSet, const VkSampler& sampler, const VkImageView& imageView, uint32_t bindingIndex, VkImageLayout layout, int arrayIndex = 0);
		static void UpdateDescriptorSet(const VkDescriptorSet& dscSet, const VkBuffer& buffer, VkDeviceSize offset, VkDeviceSize range, uint32_t bindingIndex);
	};
}