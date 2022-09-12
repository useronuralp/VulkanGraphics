#pragma once
#include "vulkan/vulkan.h"
#include "Texture.h"
#include <vector>
#include <string>
namespace Anor
{
	struct Utils
	{
		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callbackFNC);
		static std::vector<char> ReadFile(const std::string& filePath);
		static void CreateVKBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
		static uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
		static void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex);
		static void CreateSampler(const Ref<Texture>& texture, VkSampler& samplerOut);
	};
}