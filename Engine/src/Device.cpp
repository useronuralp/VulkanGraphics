#include "Device.h"
#include "DeviceRequirements.h"
#include "EngineInternal.h"
#include "PhysicalDevice.h"
#include "VulkanContext.h"

namespace
{
// False (with the feature's name printed) if it was requested but the GPU doesn't have it.
static bool CheckFeature(VkBool32 InRequested, VkBool32 InSupported, const char* InName)
{
    if (InRequested && NOT InSupported)
    {
        PrintError(std::string("Required device feature not supported: ") + InName);
        return false;
    }
    return true;
}

static bool ValidateFeatureSupport(VkPhysicalDevice InPhysicalDevice, const DeviceFeatureChain& InRequested, bool InIncludeRayTracing)
{
    // What the GPU supports, filled in by the driver. Compared against InRequested (what the renderer needs).
    DeviceFeatureChain deviceFeatureCapabilities{};
    vkGetPhysicalDeviceFeatures2(InPhysicalDevice, deviceFeatureCapabilities.LinkGetChain(InIncludeRayTracing));

    bool allSupported = true;
#define CHECK_FEATURE(Path) allSupported &= CheckFeature(InRequested.Path, deviceFeatureCapabilities.Path, #Path)
    CHECK_FEATURE(Core.features.samplerAnisotropy);
    CHECK_FEATURE(Core.features.geometryShader);
    CHECK_FEATURE(Vulkan12.bufferDeviceAddress);
    CHECK_FEATURE(Vulkan13.dynamicRendering);
    if (InIncludeRayTracing)
    {
        CHECK_FEATURE(AccelerationStructure.accelerationStructure);
        CHECK_FEATURE(RayTracingPipeline.rayTracingPipeline);
    }
#undef CHECK_FEATURE

    return allSupported;
}
} // namespace

Device::Device(const DeviceRequirements& InRequirements) : m_DeviceExtensions(InRequirements.Extensions), m_RayTracingEnabled(InRequirements.RayTracing)
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

    // Features are requested through a VkPhysicalDeviceFeatures2 pNext chain. Features2::features *is* the
    // Vulkan 1.0 VkPhysicalDeviceFeatures struct (anisotropy, geometry shaders, ...), so the 1.0 features are
    // still enabled - just from inside the chain. The spec forbids also passing pEnabledFeatures, so it stays null.
    // Copy first: the chain's pNext pointers point into the object, so we link our own copy.
    DeviceFeatureChain requestedFeatures = InRequirements.Features;
    ENSURE(
        ValidateFeatureSupport(EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), requestedFeatures, m_RayTracingEnabled),
        "The selected GPU doesn't support every device feature the renderer requires. See errors above.");

    PrintInfo("Enabling device extensions:");
    for (const char* extension : m_DeviceExtensions)
    {
        std::cout << '\t' << extension << std::endl;
    }

    // Create info for the device.
    VkDeviceCreateInfo CI{};
    CI.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    CI.pNext                   = requestedFeatures.LinkGetChain(m_RayTracingEnabled);
    CI.queueCreateInfoCount    = deviceQueueCreateInfos.size();
    CI.pQueueCreateInfos       = deviceQueueCreateInfos.data();
    CI.pEnabledFeatures        = nullptr; // must be null when pNext carries VkPhysicalDeviceFeatures2
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

    ENSURE(
        vkCreateDevice(EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &CI, nullptr, &m_Device) == VK_SUCCESS,
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
        ENSURE(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.TransferFamily, 0, &m_TransferQueue);
    }
    // Index is the same with the graphics queue.
    else
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily);
        ENSURE(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, queueIndex, &m_TransferQueue);
        queueIndex++;
    }

    // Grab a compute queue.
    // Index is unqiue.
    if (EngineInternal::GetContext()._QueueFamilies.GraphicsFamily != EngineInternal::GetContext()._QueueFamilies.ComputeFamily)
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.ComputeFamily);
        ENSURE(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.ComputeFamily, 0, &m_ComputeQueue);
    }
    // Index is the same with the graphics queue.
    else
    {
        VkQueueFamilyProperties props = GetQueueFamilyProps(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily);
        ENSURE(props.queueCount >= queueIndex, "Exceeded the maxium number of queues for this queue family");
        vkGetDeviceQueue(m_Device, EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, queueIndex, &m_ComputeQueue);
        queueIndex++;
    }
    PrintInfo("Logical device has been created.");
}

Device::~Device()
{
    vkDestroyDevice(m_Device, nullptr);
}
VkQueueFamilyProperties Device::GetQueueFamilyProps(uint64_t queueFamilyIndex)
{
    std::vector<VkQueueFamilyProperties> props;
    uint32_t                             propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &propertyCount, nullptr);

    props.resize(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(EngineInternal::GetContext().GetPhysicalDevice()->GetVKPhysicalDevice(), &propertyCount, props.data());

    return props[queueFamilyIndex];
}
