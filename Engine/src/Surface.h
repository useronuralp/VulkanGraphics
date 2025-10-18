#pragma once
#include "core.h"

#include <vector>
#include <vulkan/vulkan.h>

class Instance;
class Window;
class PhysicalDevice;

class Surface
{
   public:
    Surface(
        std::shared_ptr<Instance>       InInstance,
        std::shared_ptr<Window>         InWindow,
        std::shared_ptr<PhysicalDevice> InPhysicalDevice);
    Surface();
    ~Surface();

   public:
    VkExtent2D          GetVKExtent();
    const VkSurfaceKHR& GetVKSurface()
    {
        return _Surface;
    }
    const VkSurfaceFormatKHR& GetVKSurfaceFormat()
    {
        return _SurfaceFormat;
    }
    const VkSurfaceCapabilitiesKHR& GetVKSurfaceCapabilities()
    {
        return _Capabilities;
    }

   private:
    std::shared_ptr<Instance>       _Instance;
    std::shared_ptr<Window>         _Window;
    std::shared_ptr<PhysicalDevice> _PhysicalDevice;

    VkSurfaceKHR             _Surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       _SurfaceFormat;
    VkSurfaceCapabilitiesKHR _Capabilities;
};
