#include "Utils.h"
#include <iostream>
#include <fstream>
namespace Anor
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
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);

        //std::cout << std::filesystem::current_path() << std::endl;

        if (!file.is_open())
        {
            std::cerr << "Error:" << errno << std::endl;
            throw std::runtime_error("Failed to open file!");
        }
        // tellg(): tellg() gives you the current position of the reader head. In this case we opened the file by starting to read if from the end so it gives us the size.
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize); // Reads the contents of "file" into buffer with the help of buffer.data() pointer. The size of the reading is "fileSize";

        file.close();

        return buffer;
    }

}