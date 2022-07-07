#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include <string>
namespace Anor
{
	class Utils
	{
	public:
		static void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo, PFN_vkDebugUtilsMessengerCallbackEXT callbackFNC);
		static std::vector<char> ReadFile(const std::string& filePath);
	};
}