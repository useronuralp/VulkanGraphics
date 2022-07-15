#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <set>
#include "core.h"
namespace Anor
{
    struct QueueFamily
    {
        uint32_t Index = -1;
        VkQueueFamilyProperties Props;
    public:
        VkQueue& GetVKQueue() { return m_Queue; }
    private:
        VkQueue m_Queue;
    };


	class PhysicalDevice
	{
    public:
        const static uint32_t InvalidQueueFamilyIndex = -1;
    public:
        PhysicalDevice() = default;
        PhysicalDevice(const VkInstance& instance, const VkPhysicalDevice& physicalDevice);
    public:
        const VkPhysicalDeviceProperties&       GetVKProperties()              { return m_Properties; }
        const VkPhysicalDeviceFeatures&         GetVKFeatures()                { return m_Features; }
        const std::vector<QueueFamily>          GetQueueFamilies()             { return m_QueueFamilies; }
        const VkPhysicalDevice&                 GetVKPhysicalDevice()          { return m_PhysicalDevice; }
        const VkPhysicalDeviceMemoryProperties& GetVKDeviceMemoryProperties()  { return m_MemoryProperties; }


        uint64_t                   FindQueueFamily       (VkQueueFlags queueFlags);
        bool                       CheckPresentSupport   (uint32_t queueFamilyIndex, VkSurfaceKHR surface);
        const std::string&         GetPhysicalDeviceName();
	private:
		VkPhysicalDevice                     m_PhysicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties           m_Properties = {};
        VkPhysicalDeviceFeatures             m_Features = {};
        std::vector<QueueFamily>             m_QueueFamilies; // Storing all the queue family properties in this vector.
        std::vector<VkExtensionProperties>   m_SupportedExtensions;
        VkPhysicalDeviceMemoryProperties     m_MemoryProperties;
	};

}