#include "LogicalDevice.h"
#include "VulkanApplication.h"
#include "PhysicalDevice.h"
#include "Instance.h"
#include "Window.h"
#include "Surface.h"
namespace Anor
{
	LogicalDevice::LogicalDevice()
	{
        std::vector<QueueFamily> queueFamilies = VulkanApplication::s_PhysicalDevice->GetQueueFamilies();
        VkBool32 supported = false;
        std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

        // Create a graphics queue.
        float graphicsQueuePriority = 1.0f;
        const float* priorities     = { &graphicsQueuePriority };
        VkDeviceQueueCreateInfo QCI{};
        QCI.sType                   = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QCI.queueFamilyIndex        = VulkanApplication::s_GraphicsQueueIndex;
        QCI.queueCount              = 1;
        QCI.pQueuePriorities        = priorities;

        deviceQueueCreateInfos.push_back(QCI);

        // If the indices of graphics & present queues are not the same, we need to create a separate queue for present operations.
        if (VulkanApplication::s_GraphicsQueueIndex != VulkanApplication::s_PresentQueueIndex)
        {
            // Create a present queue.
            float presentQueuePriority = 1.0f;
            const float* priorities    = { &presentQueuePriority };
            VkDeviceQueueCreateInfo QCI{};
            QCI.sType                  = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QCI.queueFamilyIndex       = VulkanApplication::s_PresentQueueIndex;
            QCI.queueCount             = 1;
            QCI.pQueuePriorities       = priorities;

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

        VkDeviceCreateInfo CI {};
        CI.sType                    = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        CI.queueCreateInfoCount     = deviceQueueCreateInfos.size();
        CI.pQueueCreateInfos        = deviceQueueCreateInfos.data();
        CI.pEnabledFeatures         = pDeviceFeatures;
        CI.enabledExtensionCount    = m_Extensions.size();
        CI.ppEnabledExtensionNames  = m_Extensions.data();

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

        // Get a graphics queue from the device. We'll need this while using command buffers.
        vkGetDeviceQueue(m_Device, VulkanApplication::s_GraphicsQueueIndex, 0, &m_GraphicsQueue);

        // Index is Unique.
        if (VulkanApplication::s_GraphicsQueueIndex != VulkanApplication::s_PresentQueueIndex)
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_PresentQueueIndex, 0, &m_PresentQueue);
        }
        // Index is the same with the graphics queue.
        else
        {
            vkGetDeviceQueue(m_Device, VulkanApplication::s_GraphicsQueueIndex, 0, &m_PresentQueue);
        }
        std::cout << "Logical device has been created." << std::endl;
	}

	Anor::LogicalDevice::~LogicalDevice()
	{
        vkDestroyDevice(m_Device, nullptr);
	}
}