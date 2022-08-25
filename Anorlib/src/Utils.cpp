#include "Utils.h"
#include <iostream>
#include <fstream>
#include "core.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "CommandBuffer.h"
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
        
        std::string path = std::string(SOLUTION_DIR) + "Anorlib\\" + filePath;
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

    void Utils::CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, uint64_t graphicsQueueIndex)
    {
        VkCommandPool singleCmdPool;
        VkCommandBuffer singleCmdBuffer;
        CommandBuffer::Create(graphicsQueueIndex, singleCmdPool, singleCmdBuffer);
        CommandBuffer::Begin(singleCmdBuffer);

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(singleCmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        CommandBuffer::End(singleCmdBuffer);
        CommandBuffer::Submit(singleCmdBuffer);
        CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool);
    }

}