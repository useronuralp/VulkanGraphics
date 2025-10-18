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

    std::shared_ptr<Instance>       GetInstance() const;
    std::shared_ptr<LogicalDevice>  GetDevice() const;
    std::shared_ptr<PhysicalDevice> GetPhysicalDevice() const;
    std::shared_ptr<Surface>        GetSurface() const;
    // TO DO: Move this out of here;
    std::shared_ptr<Window> GetWindow() const;

    QueueFamilyIndices _QueueFamilies;

   private:
    void PickPhysicalDevice();
    void SetupQueueFamilies();
    void CreateLogicalDevice();

    VkSampleCountFlagBits GetMaxUsableSampleCount(const std::shared_ptr<PhysicalDevice>& physDevice);

    std::shared_ptr<Window>         _Window;
    std::shared_ptr<Instance>       _Instance;
    std::shared_ptr<PhysicalDevice> _PhysicalDevice;
    std::shared_ptr<Surface>        _Surface;
    std::shared_ptr<LogicalDevice>  _Device;

    VkSampleCountFlagBits _MSAASamples           = VK_SAMPLE_COUNT_1_BIT;

    std::vector<const char*> _RequiredExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
};