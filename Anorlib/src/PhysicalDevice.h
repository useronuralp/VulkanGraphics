#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>
#include <set>
#include "LogicalDevice.h"
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
    friend class Instance;
    friend class LogicalDevice;
    public:
        const static uint32_t InvalidQueueFamilyIndex = -1;
    public:
        PhysicalDevice()           = default;
        ~PhysicalDevice();
        VkPhysicalDeviceProperties       GetVKProperties()              { return m_Properties; }
        VkPhysicalDeviceFeatures         GetVKFeatures()                { return m_Features; }
        std::vector<const char*>         GetDeviceExtensions()          { return m_RequestedExtensions; }
        std::vector<QueueFamily>         GetQueueFamilies()             { return m_QueueFamilies; }
        VkPhysicalDevice                 GetVKPhysicalDevice()          { return m_PhysicalDevice; }
        VkPhysicalDeviceMemoryProperties GetVKDeviceMemoryProperties()  { return m_MemoryProperties; }


        uint32_t                   FindQueueFamily       (VkQueueFlags queueFlags);
        void                       SetDeviceExtensions   (std::vector<const char*> extensions);
        bool                       CheckPresentSupport   (uint32_t queueFamilyIndex, VkSurfaceKHR surface);
        bool                       IsExtensionSupported  (const char* extensionName);
        void                       PrintQueueFamilies();
        std::string                GetPhysicalDeviceName();
    private:
        bool                       CheckExtensionsSupport(const std::vector<const char*> extensions);
        void                       Init(const VkInstance& instance);
        PhysicalDevice(VkInstance& instance, const VkPhysicalDevice& physicalDevice, bool isVersbose = true);
	private:
		VkPhysicalDevice                     m_PhysicalDevice = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties           m_Properties = {};
        VkPhysicalDeviceFeatures             m_Features = {};
        std::vector<QueueFamily>             m_QueueFamilies; // Storing all the queue family properties in this vector.
        bool							     m_IsVerbose;
        std::vector<VkExtensionProperties>   m_SupportedExtensions;
        std::vector<const char*>             m_RequestedExtensions;
        VkPhysicalDeviceMemoryProperties     m_MemoryProperties;
	};

}