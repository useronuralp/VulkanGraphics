#include "Instance.h"
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "Surface.h"
#include "VulkanContext.h"
#include "Window.h"

#include <iostream>

VulkanContext::~VulkanContext()
{
    Shutdown();
}

void VulkanContext::Init()
{
    // 1. Create window
    auto test = make_s<Window>("Vulkan Engine", 1920, 1080);
    _Window   = test;

    // 2. Create instance
    _Instance = make_s<Instance>();

    // 3. Enumerate and pick physical device
    PickPhysicalDevice();

    // 4. Query MSAA
    _MSAASamples = GetMaxUsableSampleCount(_PhysicalDevice);

    // 5. Create surface (depends on instance + window)
    _Surface = make_s<Surface>(_Instance, _Window, _PhysicalDevice);

    // 6. Setup queue families
    SetupQueueFamilies();

    // 7. Create logical device
    CreateLogicalDevice();

    std::cout << "VulkanContext initialized successfully." << std::endl;
}

void VulkanContext::PickPhysicalDevice()
{
    std::vector<PhysicalDevice> devices;
    devices = _Instance->GetVKPhysicalDevices();

    // Find a suitable PHYSICAL (GPU) device here.
    bool found = false;

    std::cout << "Searching for a DISCREETE graphics card..." << std::endl;
    // First, check for a dedicated graphics card.
    for (auto& device : devices)
    {
        if (device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            _PhysicalDevice = make_s<PhysicalDevice>(device);
            found           = true;
            std::cout << "Found a suitable DISCREETE graphics card. \n\t Card "
                         "Name ---> "
                      << _PhysicalDevice->GetVKProperties().deviceName << std::endl;
            break;
        }
    }
    // If still haven't found a GPU, we pick the first available one without
    // looking at any other condition.
    if (!found)
    {
        std::cout << "Could not find a DISCREETE graphics card.\n" << std::endl;
        std::cout << "Searching for an INTEGRATED graphics card... " << std::endl;
        for (auto& device : devices)
        {
            _PhysicalDevice = make_s<PhysicalDevice>(device);
            found           = true;
            std::cout << "Found a suitable INTEGRATED graphics card. \n\t Card "
                         "Name ---> "
                      << _PhysicalDevice->GetVKProperties().deviceName << std::endl;
            break;
        }
    }

    ASSERT(found, "Could not find a GPU.");
}

void VulkanContext::CreateLogicalDevice()
{
    _Device = make_s<LogicalDevice>(_RequiredExtensions);
}

void VulkanContext::SetupQueueFamilies()
{
    // Here, fetch the first queue family that supports graphics operations.
    uint64_t index                = _PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    _QueueFamilies.GraphicsFamily = index;

    // Check whether the graphics queue we just got also supports present
    // operations.
    ASSERT(
        _PhysicalDevice->CheckPresentSupport(_QueueFamilies.GraphicsFamily, _Surface->GetVKSurface()),
        "Present operations are not supported by the graphics queue. Might "
        "want to search for it manually.");

    // Leaving this part here in case you want to debug the quesue families.
    std::vector<VkQueueFamilyProperties> props;
    uint32_t                             propertyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, nullptr);

    props.resize(propertyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, props.data());

    // Try to find a compute queue family.
    index                        = _PhysicalDevice->FindQueueFamily(VK_QUEUE_COMPUTE_BIT);
    _QueueFamilies.ComputeFamily = index;

    // Try to find a transfer queue family.
    index                         = _PhysicalDevice->FindQueueFamily(VK_QUEUE_TRANSFER_BIT);
    _QueueFamilies.TransferFamily = index;
}

Ref<Instance> VulkanContext::GetInstance() const
{
    return _Instance;
}

Ref<LogicalDevice> VulkanContext::GetDevice() const
{
    return _Device;
}

Ref<PhysicalDevice> VulkanContext::GetPhysicalDevice() const
{
    return _PhysicalDevice;
}

Ref<Surface> VulkanContext::GetSurface() const
{
    return _Surface;
}

Ref<Window> VulkanContext::GetWindow() const
{
    return _Window;
}

VkSampleCountFlagBits VulkanContext::GetMaxUsableSampleCount(const Ref<PhysicalDevice>& physDevice)
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physDevice->GetVKPhysicalDevice(), &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanContext::Shutdown()
{
    if (_Device)
    {
        vkDeviceWaitIdle(_Device->GetVKDevice()); // Wait for all GPU work to finish
    }

    // 2. Reset objects in reverse creation order
    _Device.reset(); // LogicalDevice first (frees queues, semaphores, command pools)
    _Surface.reset(); // Surface next (depends on instance + window)
    _PhysicalDevice.reset(); // Usually safe to reset next
    _Instance.reset(); // Vulkan instance
    _Window.reset(); // Destroy the window last

    std::cout << "VulkanContext shutdown complete." << std::endl;
}
