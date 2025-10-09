#pragma once
#include "core.h"
// External
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <set>
namespace OVK
{
    struct QueueFamily
    {
        uint32_t                Index = -1;
        VkQueueFamilyProperties Props;
    public:
        VkQueue& GetVKQueue() { return m_Queue; }
    private:
        VkQueue m_Queue;
    };


	class PhysicalDevice
	{
    public:
        const static uint32_t s_InvalidQueueFamilyIndex = -1;
    public:
        // Constructors / Destructors
        PhysicalDevice() = default;
        PhysicalDevice(const VkInstance& instance, const VkPhysicalDevice& physicalDevice);
    public:
        const VkPhysicalDeviceProperties&       GetVKProperties()              { return m_Properties; }
        const VkPhysicalDeviceFeatures&         GetVKFeatures()                { return m_Features; }
        const std::vector<QueueFamily>          GetQueueFamilies()             { return m_QueueFamilies; }
        const VkPhysicalDevice&                 GetVKPhysicalDevice()          { return m_PhysicalDevice; }
        const VkPhysicalDeviceMemoryProperties& GetVKDeviceMemoryProperties()  { return m_MemoryProperties; }
        const std::string&                      GetPhysicalDeviceName();
    public:
        uint64_t                   FindQueueFamily       (VkQueueFlags queueFlags);
        bool                       CheckPresentSupport   (uint32_t queueFamilyIndex, VkSurfaceKHR surface);
	private:
		VkPhysicalDevice                     m_PhysicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties           m_Properties     = {};
        VkPhysicalDeviceFeatures             m_Features       = {};
        std::vector<QueueFamily>             m_QueueFamilies; // Storing all the queue family properties in this vector.
        std::vector<VkExtensionProperties>   m_SupportedExtensions;
        VkPhysicalDeviceMemoryProperties     m_MemoryProperties;

	};

}