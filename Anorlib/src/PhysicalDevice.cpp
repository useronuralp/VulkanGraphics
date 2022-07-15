#include "PhysicalDevice.h"
#include "Instance.h"
#include <string>
namespace Anor
{
    PhysicalDevice::PhysicalDevice(const VkInstance& instance, const VkPhysicalDevice& physicalDevice)
        :m_PhysicalDevice(physicalDevice)
    {
        // Enumerate neccessary properties.
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &m_Properties);
        vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_Features);
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &m_MemoryProperties);

        uint32_t queueFamilyCount = 0;
        std::vector<VkQueueFamilyProperties> props;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);

        ASSERT(queueFamilyCount > 0, "Could not find any queue families in this physical device.");
        props.resize(queueFamilyCount);

        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, props.data());
        ASSERT(queueFamilyCount == props.size(), "There has been a problem with the queue families.");

        for (int i = 0; i < queueFamilyCount; i++)
        {
            QueueFamily family;
            family.Index = i;
            family.Props = props[i];
            m_QueueFamilies.push_back(family);
        }

        // Find supported extensions.
        uint32_t extensionCount = 0;

        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
        ASSERT(extensionCount > 0, "Physical Device doesn't support ANY extensions.");

        m_SupportedExtensions.resize(extensionCount);

        // Store the supported extenions in the member variable.
        ASSERT(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, m_SupportedExtensions.data()) == VK_SUCCESS, "Failed to enumerate device extension properties.");

        //std::cout << "Found a suitable GPU with Vulkan support --->  " << m_Properties.deviceName << std::endl;
    }

    uint64_t PhysicalDevice::FindQueueFamily(VkQueueFlags queueFlags)
    {
        uint64_t familyIndex = InvalidQueueFamilyIndex; // init to an invalid index. -1 is invaild.

        VkQueueFlags queueFlagsOptional = queueFlags;
        if (queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            queueFlags &= ~VK_QUEUE_TRANSFER_BIT;
            queueFlagsOptional = queueFlags | VK_QUEUE_TRANSFER_BIT;
        }

        // First, check if the flags match exactly.
        for (const auto& family : m_QueueFamilies)
        {
            if (family.Props.queueFlags == queueFlags || family.Props.queueFlags == queueFlagsOptional)
            {
                familyIndex = family.Index; // If a family queue include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT then VK_QUEUE_TRANSFER_BIT is also supported.

                std::cout << "Found a dedicated queue family that ONLY supports the requested flags!" << std::endl;
                break;
            }
        }
        
        // If the above check fails, check if a queue family INCLUDES the preferred flags, they don't need to match exactly.
        if (familyIndex == InvalidQueueFamilyIndex)
        {
            for (const auto& family : m_QueueFamilies)
            {
                if ((family.Props.queueFlags & queueFlags) == queueFlags)
                {
                    familyIndex = family.Index;
                    std::cout << "Found a queue family that INCLUDES requested flags." << std::endl;
                    break;
                }
            }
        }
        ASSERT(familyIndex != InvalidQueueFamilyIndex, "Could not find a queue family with the desired queue families.");
        return familyIndex;
    }
    const std::string& PhysicalDevice::GetPhysicalDeviceName()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
        return std::string(props.deviceName);
    }
    bool PhysicalDevice::CheckPresentSupport(uint32_t queueFamilyIndex, VkSurfaceKHR surface)
    {
        VkBool32 PresentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, queueFamilyIndex, surface, &PresentSupport);
        return PresentSupport == VK_TRUE;
    }
}