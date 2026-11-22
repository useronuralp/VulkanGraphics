#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

struct DeviceRequirements;

class Device
{
   public:
    Device(const DeviceRequirements& InRequirements);
    ~Device();

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

    // Decides whether the RT feature structs are linked into the create-info chain. The device only
    // *enables* ray tracing; the entry points are loaded by the path tracer, their only consumer.
    bool m_RayTracingEnabled = false;
};
