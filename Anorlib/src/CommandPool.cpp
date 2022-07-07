#include "CommandPool.h"
#include "LogicalDevice.h"
#include <iostream>
namespace Anor
{
	CommandPool::CommandPool(CreateInfo& createInfo)
        :m_Device(createInfo.pLogicalDevice)
	{
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = createInfo.QueueFamilyIndex;

        if (vkCreateCommandPool(m_Device->GetVKDevice(), &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
            __debugbreak();
        }
	}

    CommandPool::~CommandPool()
    {
        vkDestroyCommandPool(m_Device->GetVKDevice(), m_CommandPool, nullptr);
    }

}