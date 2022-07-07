#include "PhysicalDevice.h"
#include "Instance.h"
#include "core.h"
#include <string>
namespace Anor
{
    PhysicalDevice::PhysicalDevice(VkInstance& instance, const VkPhysicalDevice& physicalDevice, bool isVersbose)
        :m_PhysicalDevice(physicalDevice), m_IsVerbose(isVersbose)
    {
        Init(instance);
    }
    PhysicalDevice::~PhysicalDevice()
    {
    }
    uint32_t PhysicalDevice::FindQueueFamily(VkQueueFlags queueFlags)
    {
        uint32_t familyIndex = InvalidQueueFamilyIndex; // init to an invalid index. -1 is invaild.
        uint32_t queueFamilyCount = 0;

        VkQueueFlags queueFlagsOptional = queueFlags;
        if (queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
        {
            queueFlags &= ~VK_QUEUE_TRANSFER_BIT;
            queueFlagsOptional = queueFlags | VK_QUEUE_TRANSFER_BIT;
        }

        for (const auto& family : m_QueueFamilies)
        {
            // First, check if the flags match exactly.
            if (family.Props.queueFlags == queueFlags || family.Props.queueFlags == queueFlagsOptional)
            {
                familyIndex = family.Index; // If a family queue include VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT then VK_QUEUE_TRANSFER_BIT is also supported.
                if (m_IsVerbose)
                {
                    std::cout << "Found a dedicated queue family!" << std::endl;
                }
                break;
            }
        }
        
        // if the family index is still invalid meaning there has been no assignment.
        if (familyIndex == InvalidQueueFamilyIndex)
        {
            // Secondly, check to see if the requested flags are at least there. It doesn't need to match perfectly in this case.
            for (const auto& family : m_QueueFamilies)
            {
                if ((family.Props.queueFlags & queueFlags) == queueFlags)
                {
                    familyIndex = family.Index;
                    if (m_IsVerbose)
                    {
                        std::cout << "Found a queue family that supports requested flags." << std::endl;
                    }
                    break;
                }
            }
        }
        return familyIndex;
    }
    void PhysicalDevice::SetDeviceExtensions(std::vector<const char*> extensions)
    {
        CheckExtensionsSupport(extensions);
        m_RequestedExtensions = extensions;
    }
    bool PhysicalDevice::IsExtensionSupported(const char* extensionName)
    {
        for (const auto& extension : m_SupportedExtensions)
            if (strcmp(extension.extensionName, extensionName) == 0)
            {
                return true;
            }

        return false;
    }
    void PhysicalDevice::PrintQueueFamilies()
    {
        int i = 1;
        for (const auto& family : m_QueueFamilies)
        {
            std::cout << "Queue number " << i << "\nCreateable queue number: " + std::to_string(family.Props.queueCount) << std::endl;
            std::cout << "Queue flags: " + std::to_string(family.Props.queueFlags) << std::endl;
            i++;
        }
    }
    std::string PhysicalDevice::GetPhysicalDeviceName()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
        return std::string(props.deviceName);
    }
    bool PhysicalDevice::CheckExtensionsSupport(const std::vector<const char*> extensions)
    {
        std::cout << "Now checking for physical device extensions support for device: " << m_Properties.deviceName << std::endl;
        std::set<std::string> requiredExtensions(extensions.begin(), extensions.end());

        for (const auto& extension : m_SupportedExtensions)
        {
            if (requiredExtensions.find(extension.extensionName) != requiredExtensions.end())
            {
                if (m_IsVerbose)
                {
                    std::cout << "Extension: " << extension.extensionName << " is supported." << std::endl;
                }
            }
            auto pointedExtension = requiredExtensions.erase(extension.extensionName);
        }
        if (!requiredExtensions.empty())
        {
            std::cerr << "Requested extensions are not supported." << std::endl;
            __debugbreak();
        }
        return requiredExtensions.empty(); // If the vector is empty that means all the required extensions have been found.
    }
    bool PhysicalDevice::CheckPresentSupport(uint32_t queueFamilyIndex, VkSurfaceKHR surface)
    {
        VkBool32 PresentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_PhysicalDevice, queueFamilyIndex, surface, &PresentSupport);
        return PresentSupport == VK_TRUE;
    }
    void PhysicalDevice::Init(const VkInstance& instance)
    {
        if (m_PhysicalDevice == VK_NULL_HANDLE)
        {
            std::cerr << "Physical Instance handle has been passed empty." << std::endl;
            __debugbreak();
        }

        vkGetPhysicalDeviceProperties       (m_PhysicalDevice, &m_Properties);
        vkGetPhysicalDeviceFeatures         (m_PhysicalDevice, &m_Features);
        vkGetPhysicalDeviceMemoryProperties (m_PhysicalDevice, &m_MemoryProperties);

        uint32_t queueFamilyCount = 0;
        std::vector<VkQueueFamilyProperties> props;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount > 0)
        {
            props.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, props.data());
            if (queueFamilyCount != props.size())
            {
                std::cerr << "There has been a problem with the queue families." << std::endl;
                __debugbreak();
            }
            else
            {
                for (int i = 0; i < queueFamilyCount; i++)
                {
                    QueueFamily family;
                    family.Index = i;
                    family.Props = props[i];
                    m_QueueFamilies.push_back(family);
                }
            }
        }
        else
        {
            std::cerr << "Found 0 queue families." << std::endl;
            __debugbreak();
        }

        // Find supported extensions.
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, nullptr);
        if (extensionCount > 0)
        {
            m_SupportedExtensions.resize(extensionCount);
            auto res = vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &extensionCount, m_SupportedExtensions.data());
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to enumerate device extension properties." << std::endl;
                __debugbreak();
            }
        }

        if (m_IsVerbose)
        {
            std::cout << "Found a suitable GPU with Vulkan support --->  " << m_Properties.deviceName << std::endl;
        }
    }
}