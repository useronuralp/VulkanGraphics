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
        Ref<Instance>       InInstance,
        Ref<Window>         InWindow,
        Ref<PhysicalDevice> InPhysicalDevice);
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
    Ref<Instance>       _Instance;
    Ref<Window>         _Window;
    Ref<PhysicalDevice> _PhysicalDevice;

    VkSurfaceKHR             _Surface = VK_NULL_HANDLE;
    VkSurfaceFormatKHR       _SurfaceFormat;
    VkSurfaceCapabilitiesKHR _Capabilities;
};
