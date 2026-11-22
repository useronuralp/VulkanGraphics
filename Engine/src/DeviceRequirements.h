#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

// -------------------------------------------------------------------------------------------------
// DeviceFeatureChain
// -------------------------------------------------------------------------------------------------
// Since Vulkan 1.1, device features are described by a linked list of structs hanging off
// VkPhysicalDeviceFeatures2::pNext. Every struct in the list has the same header
//     { VkStructureType sType; void* pNext; ... }
// and the driver walks the list by reading sType to know what each node is.
//
// The same chain shape is used for two things:
//   1. vkGetPhysicalDeviceFeatures2 -> "what does this GPU support?"   (driver fills the VkBool32s)
//   2. VkDeviceCreateInfo::pNext    -> "what do I want enabled?"       (we fill the VkBool32s)
//
// We keep one of each struct by value and wire the pNext pointers on demand with Link().
// Because the pointers point *into this object*, any copy/move invalidates them, so always call
// Link() on the object you are about to hand to Vulkan, right before handing it over.
// -------------------------------------------------------------------------------------------------
struct DeviceFeatureChain
{
    // Base. This address is returned
    VkPhysicalDeviceFeatures2                        Core{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };

    // Tail nodes
    VkPhysicalDeviceVulkan11Features                 Vulkan11{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES };
    VkPhysicalDeviceVulkan12Features                 Vulkan12{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    VkPhysicalDeviceVulkan13Features                 Vulkan13{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    VkPhysicalDeviceAccelerationStructureFeaturesKHR AccelerationStructure{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR    RayTracingPipeline{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };

    // InIncludeRayTracing: extension feature structs may only appear in VkDeviceCreateInfo's chain if
    // their extension is also enabled, so the RT nodes are only linked when the RT extensions are requested.
    VkPhysicalDeviceFeatures2* LinkGetChain(bool InIncludeRayTracing)
    {
        Core.pNext                  = &Vulkan11;
        Vulkan11.pNext              = &Vulkan12;
        Vulkan12.pNext              = &Vulkan13;
        Vulkan13.pNext              = InIncludeRayTracing ? &AccelerationStructure : nullptr;
        AccelerationStructure.pNext = InIncludeRayTracing ? &RayTracingPipeline : nullptr;
        RayTracingPipeline.pNext    = nullptr;
        return &Core;
    }
};

// -------------------------------------------------------------------------------------------------
// DeviceRequirements
// -------------------------------------------------------------------------------------------------
// What a renderer needs from the logical device. Extensions and features are fixed at vkCreateDevice
// time and can't be turned on later, so the renderer has to declare them *before* the VulkanContext
// is initialised. Each renderer exposes a static GetDeviceRequirements() for exactly this reason.
// -------------------------------------------------------------------------------------------------
struct DeviceRequirements
{
    std::vector<const char*> Extensions;
    DeviceFeatureChain       Features;

    // Links the RT feature structs into the chain and loads the RT entry points after device creation.
    bool RayTracing = false;

    // The feature chain always contains VkPhysicalDeviceVulkan13Features, which is only valid on 1.3 devices.
    uint32_t MinApiVersion = VK_API_VERSION_1_3;
};
