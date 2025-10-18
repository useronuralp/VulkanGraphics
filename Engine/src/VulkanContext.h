#pragma once

#include <vector>
#include <vulkan/vulkan.h>

class Instance;
class PhysicalDevice;
class Surface;
class Window;
class LogicalDevice;

struct QueueFamilyIndices
{
    uint32_t GraphicsFamily = UINT32_MAX;
    uint32_t PresentFamily  = UINT32_MAX;
    uint32_t TransferFamily = UINT32_MAX;
    uint32_t ComputeFamily  = UINT32_MAX;

    // What is this?
    bool isComplete() const
    {
        return GraphicsFamily != UINT32_MAX && PresentFamily != UINT32_MAX;
    }
};

class VulkanContext
{
   public:
    VulkanContext() = default;
    ~VulkanContext();

    void Init();
    void Shutdown();

    Ref<Instance>       GetInstance() const;
    Ref<LogicalDevice>  GetDevice() const;
    Ref<PhysicalDevice> GetPhysicalDevice() const;
    Ref<Surface>        GetSurface() const;
    // TO DO: Move this out of here;
    Ref<Window> GetWindow() const;

    QueueFamilyIndices _QueueFamilies;

   private:
    void PickPhysicalDevice();
    void SetupQueueFamilies();
    void CreateLogicalDevice();

    VkSampleCountFlagBits GetMaxUsableSampleCount(const Ref<PhysicalDevice>& physDevice);

    Ref<Window>         _Window;
    Ref<Instance>       _Instance;
    Ref<PhysicalDevice> _PhysicalDevice;
    Ref<Surface>        _Surface;
    Ref<LogicalDevice>  _Device;

    VkSampleCountFlagBits _MSAASamples           = VK_SAMPLE_COUNT_1_BIT;

    std::vector<const char*> _RequiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};