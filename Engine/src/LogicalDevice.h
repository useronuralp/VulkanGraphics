#pragma once
#include "core.h"
// External
#include <vector>
#include <vulkan/vulkan.h>

class LogicalDevice
{
   public:
    LogicalDevice(std::vector<const char*> extensions);
    ~LogicalDevice();

   public:
    const VkDevice& GetVKDevice()
    {
        return m_Device;
    }
    const VkQueue& GetGraphicsQueue()
    {
        return m_GraphicsQueue;
    }
    const VkQueue& GetTransferQueue()
    {
        return m_TransferQueue;
    }

   private:
    VkQueueFamilyProperties GetQueueFamilyProps(uint64_t queueFamilyIndex);

   private:
    VkDevice m_Device        = VK_NULL_HANDLE;
    VkQueue  m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue  m_TransferQueue = VK_NULL_HANDLE;
    VkQueue  m_ComputeQueue  = VK_NULL_HANDLE;

    std::vector<const char*> m_Layers;
    std::vector<const char*> m_DeviceExtensions;
};
