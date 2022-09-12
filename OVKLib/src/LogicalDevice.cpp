#include "LogicalDevice.h"
#include "VulkanApplication.h"
#include "PhysicalDevice.h"
#include "Instance.h"
#include "Window.h"
#include "Surface.h"
namespace Anor
{
    LogicalDevice::LogicalDevice(std::vector<const char*> extensions)
        : m_DeviceExtensions(extensions)
	{
        // Fetch queue families.
        std::vector<QueueFamily> queueFamilies = VulkanApplication::s_PhysicalDevice->GetQueueFamilies();
        VkBool32 supported = false;
        std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

        // Create a graphics queue.
        std::vector<float> priorities;
        VkDeviceQueueCreateInfo QCI{};
        QCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QCI.queueFamilyIndex        = VulkanApplication::s_GraphicsQueueFamily;

        VkQueueFamilyProperties props = GetQueueFamilyProps(VulkanApplication::s_GraphicsQueueFamily);
        priorities.assign(props.queueCount, 1.0f);

        QCI.queueCount              = props.queueCount;
        QCI.pQueuePriorities        = priorities.data();

        deviceQueueCreateInfos.push_back(QCI);

        // WARNING: THE PART WHERE QUEUE FAMILY INDICES ARE NOT SAME HAS NOT BEEN TESTED PROPERLY. THIS MIGHT CAUSE CRASHES IN YOUR PROGRAM.

        // If the indices of graphics & present queues are not the same, we need to create a separate queue for present operations.
        if (VulkanApplication::s_GraphicsQueueFamily != VulkanApplication::s_PresentQueueFamily)
        {
            // Create a present queue.
            std::vector<float> priorities;
            VkDeviceQueueCreateInfo QCI{};
            QCI.sType                  = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QCI.queueFamilyIndex       = VulkanApplication::s_PresentQueueFamily;

            VkQueueFamilyProperties props = GetQueueFamilyProps(VulkanApplication::s_PresentQueueFamily);
            priorities.assign(props.queueCount, 1.0f);

            QCI.queueCount             = props.queueCount;
            QCI.pQueuePriorities       = priorities.data();

            deviceQueueCreateInfos.push_back(QCI);
        }

        // If the indices of graphics & compute queues are not the same, we need to create a separate queue for present operations.
        if (VulkanApplication::s_GraphicsQueueFamily != VulkanApplication::s_ComputeQueueFamily)
        {
            // Create a present queue.
            std::vector<float> priorities;
            VkDeviceQueueCreateInfo QCI{};
            QCI.sType                     = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QCI.queueFamilyIndex          = VulkanApplication::s_ComputeQueueFamily;

            VkQueueFamilyProperties props = GetQueueFamilyProps(VulkanApplication::s_ComputeQueueFamily);
            priorities.assign(props.queueCount, 1.0f);

            QCI.queueCount                = props.queueCount;
            QCI.pQueuePriorities          = priorities.data();

            deviceQueueCreateInfos.push_back(QCI);
        }

        // Check Anisotrophy support.
        ASSERT(VulkanApplication::s_PhysicalDevice->GetVKFeatures().samplerAnisotropy, "Anisotropy is not supported on your GPU.");

        // Enable Anisotropy.
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = true;

        VkPhysicalDeviceFeatures pDeviceFeatures[] =
        {
            deviceFeatures
        };
        
        // Create info for the device.
        VkDeviceCreateInfo CI {};
        CI.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CI.queueCreateInfoCount     = deviceQueueCreateInfos.size();
        CI.pQueueCreateInfos        = deviceQueueCreateInfos.data();
        CI.pEnabledFeatures         = pDeviceFeatures;
        CI.enabledExtensionCount    = m_DeviceExtensions.size();
        CI.ppEnabledExtensionNames  = m_DeviceExtensions.data();

        if (m_Layers.size() == 0)
        {
            CI.enabledLayerCount = 0;
        }
        else
        {
            CI.ppEnabledLayerNames = m_Layers.data();
            CI.enabledLayerCount = m_Layers.size();
        }

        ASSERT(vkCreateDevice(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), &CI, nullptr, &m_Device) == VK_SUCCESS, "Failed to create logical device!");

        int queueIndex = 0;

        // Get a graphics queue from the device. We'll need this while using command buffers.
        vkGetDeviceQueue(m_Device, VulkanApplication::s_GraphicsQueueFamily, queueIndex, &m_GraphicsQueue);
        queueIndex++;
        // Index is Unique.
        if (VulkanApplication::s_GraphicsQueueFamily != VulkanApplication::s_PresentQueueFamily)
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_PresentQueueFamily, 0, &m_PresentQueue);
        }
        // Index is the same with the graphics queue.
        else
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_GraphicsQueueFamily, queueIndex, &m_PresentQueue);
            queueIndex++;
        }

        // Grab a compute queue.
        // Index is unqiue.
        if (VulkanApplication::s_GraphicsQueueFamily != VulkanApplication::s_ComputeQueueFamily)
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_ComputeQueueFamily, 0, &m_ComputeQueue);
        }
        // Index is the same with the graphics queue.
        else
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_GraphicsQueueFamily, queueIndex, &m_ComputeQueue);
            queueIndex++;
        }
        std::cout << "Logical device has been created." << std::endl;
	}

	Anor::LogicalDevice::~LogicalDevice()
	{
        vkDestroyDevice(m_Device, nullptr);
	}
    VkQueueFamilyProperties LogicalDevice::GetQueueFamilyProps(uint64_t queueFamilyIndex)
    {
        std::vector<VkQueueFamilyProperties> props;
        uint32_t propertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, nullptr);

        props.resize(propertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(VulkanApplication::s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, props.data());

        return props[queueFamilyIndex];
    }
}