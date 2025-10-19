#include "EngineInternal.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Surface.h"
#include "VulkanContext.h"
#include "Window.h"
LogicalDevice::LogicalDevice(std::vector<const char*> extensions) : m_DeviceExtensions(extensions)
{
    // Fetch queue families.
    std::vector<QueueFamily>             queueFamilies = EngineInternal::GetContext().GetPhysicalDevice()->GetQueueFamilies();
    VkBool32                             supported     = false;
    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfos;

    // Create a graphics queue.
    std::vector<float>      priorities;
    VkDeviceQueueCreateInfo QCI{};
    QCI.sType                     = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    QCI.queueFamilyIndex          = EngineInternal::GetContext()._QueueFamilies.GraphicsFamily;

    VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily);
    priorities.assign(props.queueCount, 1.0f);

    QCI.queueCount       = props.queueCount;
    QCI.pQueuePriorities = priorities.data();

    deviceQueueCreateInfos.push_back(QCI);

    // WARNING: THE PART WHERE QUEUE FAMILY INDICES ARE NOT SAME HAS NOT BEEN
    // TESTED PROPERLY. THIS MIGHT CAUSE CRASHES IN YOUR PROGRAM.

    // If the indices of graphics & transfer queues are not the same, we need to
    // create a separate queue for present operations.
    if (EngineInternal::GetContext()._QueueFamilies.GraphicsFamily != EngineInternal::GetContext()._QueueFamilies.TransferFamily)
    {
        // Create a present queue.
        std::vector<float>      priorities;
        VkDeviceQueueCreateInfo QCI{};
        QCI.sType                     = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QCI.queueFamilyIndex          = EngineInternal::GetContext()._QueueFamilies.TransferFamily;

        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.TransferFamily);
        priorities.assign(props.queueCount, 1.0f);

        QCI.queueCount       = props.queueCount;
        QCI.pQueuePriorities = priorities.data();

        deviceQueueCreateInfos.push_back(QCI);
    }

    // If the indices of graphics & compute queues are not the same, we need to
    // create a separate queue for present operations.
    if (EngineInternal::GetContext()._QueueFamilies.GraphicsFamily != EngineInternal::GetContext()._QueueFamilies.ComputeFamily)
    {
        // Create a present queue.
        std::vector<float>      priorities;
        VkDeviceQueueCreateInfo QCI{};
        QCI.sType                     = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        QCI.queueFamilyIndex          = EngineInternal::GetContext()._QueueFamilies.ComputeFamily;

        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.ComputeFamily);
        priorities.assign(props.queueCount, 1.0f);

        QCI.queueCount       = props.queueCount;
        QCI.pQueuePriorities = priorities.data();

        deviceQueueCreateInfos.push_back(QCI);
    }

    // Check Anisotrophy support.
    ASSERT(
        EngineInternal::GetContext().GetPhysicalDevice()->GetVKFeatures().samplerAnisotropy,
        "Anisotropy is not supported on your GPU.");

    // Enable Anisotropy.
    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.geometryShader    = VK_TRUE;

    constexpr VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceFeatures pDeviceFeatures[] = { deviceFeatures };

    // Create info for the device.
    VkDeviceCreateInfo CI{};
    CI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    CI.pNext                   = &dynamic_rendering_feature;
    CI.queueCreateInfoCount    = deviceQueueCreateInfos.size();
    CI.pQueueCreateInfos       = deviceQueueCreateInfos.data();
    CI.pEnabledFeatures        = pDeviceFeatures;
    CI.enabledExtensionCount   = m_DeviceExtensions.size();
    CI.ppEnabledExtensionNames = m_DeviceExtensions.data();

    if (m_Layers.size() == 0)
    {
        CI.enabledLayerCount = 0;
    }
    else
    {
        CI.ppEnabledLayerNames = m_Layers.data();
        CI.enabledLayerCount   = m_Layers.size();
    }

    ASSERT(
        vkCreateDevice(EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &CI, nullptr, &m_Device) ==
            VK_SUCCESS,
        "Failed to create logical device!");

    int queueIndex = 0;

    // Get a graphics queue from the device. We'll need this while using command
    // buffers.
    vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, queueIndex, &m_GraphicsQueue);
    queueIndex++;
    // Index is Unique.
    if (EngineInternal::GetContext()._QueueFamilies.GraphicsFamily != EngineInternal::GetContext()._QueueFamilies.TransferFamily)
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.TransferFamily);
        ASSERT(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.TransferFamily, 0, &m_TransferQueue);
    }
    // Index is the same with the graphics queue.
    else
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily);
        ASSERT(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, queueIndex, &m_TransferQueue);
        queueIndex++;
    }

    // Grab a compute queue.
    // Index is unqiue.
    if (EngineInternal::GetContext()._QueueFamilies.GraphicsFamily != EngineInternal::GetContext()._QueueFamilies.ComputeFamily)
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.ComputeFamily);
        ASSERT(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.ComputeFamily, 0, &m_ComputeQueue);
    }
    // Index is the same with the graphics queue.
    else
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily);
        ASSERT(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, queueIndex, &m_ComputeQueue);
        queueIndex++;
    }
    PrintInfo("Logical device has been created.");
}

LogicalDevice::~LogicalDevice()
{
    vkDestroyDevice(m_Device, nullptr);
}
VkQueueFamilyProperties LogicalDevice::GetQueueFamilyProps(uint64_t queueFamilyIndex)
{
    std::vector<VkQueueFamilyProperties> props;
    uint32_t                             propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &propertyCount, nullptr);

    props.resize(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &propertyCount, props.data());

    return props[queueFamilyIndex];
}
