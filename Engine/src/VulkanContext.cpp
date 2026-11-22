#include "Device.h"
#include "Instance.h"
#include "PhysicalDevice.h"
#include "Surface.h"
#include "VulkanContext.h"
#include "Window.h"

VulkanContext::~VulkanContext()
{
    Shutdown();
}

void VulkanContext::Init(const DeviceRequirements& InRequirements)
{
    _Requirements = InRequirements;

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

    PrintInfo("VulkanContext initialized successfully.");
}

void VulkanContext::PickPhysicalDevice()
{
    std::vector<PhysicalDevice> devices = _Instance->GetVKPhysicalDevices();

    // A GPU is only a candidate if it can actually run the selected renderer. Without this, a laptop
    // with an RT-capable dGPU and a non-RT iGPU could pick the wrong one and fail at vkCreateDevice.
    auto meetsRequirements = [&](PhysicalDevice& InDevice) -> bool
    {
        const auto& props = InDevice.GetVKProperties();
        bool        ok    = true;

        if (props.apiVersion < _Requirements.MinApiVersion)
        {
            PrintWarning(std::string(props.deviceName) + " skipped: Vulkan API version too old.");
            ok = false;
        }

        for (const char* extension : _Requirements.Extensions)
        {
            if (!InDevice.SupportsExtension(extension))
            {
                PrintWarning(std::string(props.deviceName) + " skipped: missing device extension " + extension);
                ok = false;
            }
        }
        return ok;
    };

    // Pass 0: discrete GPUs only. Pass 1: everything else (integrated, virtual, CPU).
    for (int pass = 0; pass < 2 && !_PhysicalDevice; pass++)
    {
        for (auto& device : devices)
        {
            const bool isDiscrete = device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            // Todo: convoluted coniditon, make more human friendly to read.
            if ((pass == 0) != isDiscrete || !meetsRequirements(device))
            {
                continue;
            }

            _PhysicalDevice = make_s<PhysicalDevice>(device);
            PrintInfo(std::string("Selected ") + (isDiscrete ? "DISCRETE" : "NON-DISCRETE") + " GPU: " + device.GetVKProperties().deviceName);
            break;
        }
    }

    ENSURE(_PhysicalDevice != nullptr, "Could not find a GPU that meets the renderer's requirements. See warnings above.");
}

void VulkanContext::CreateLogicalDevice()
{
    _Device = make_s<Device>(_Requirements);
}

void VulkanContext::SetupQueueFamilies()
{
    // Here, fetch the first queue family that supports graphics operations.
    uint64_t index                = _PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
    _QueueFamilies.GraphicsFamily = index;

    // Check whether the graphics queue we just got also supports present
    // operations.
    ENSURE(
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

Ref<Device> VulkanContext::GetDevice() const
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

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
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
    _Device.reset(); // Device first (frees queues, semaphores, command pools)
    _Surface.reset(); // Surface next (depends on instance + window)
    _PhysicalDevice.reset(); // Usually safe to reset next
    _Instance.reset(); // Vulkan instance
    _Window.reset(); // Destroy the window last

    PrintInfo("VulkanContext shutdown complete. \n");
}
