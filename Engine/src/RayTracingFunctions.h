#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

// -------------------------------------------------------------------------------------------------
// RayTracingFunctions
// -------------------------------------------------------------------------------------------------
// The Vulkan loader library you link against (vulkan-1.lib / libvulkan.so) only exports core and
// WSI (surface/swapchain) entry points. Calling e.g. vkCmdTraceRaysKHR directly compiles - the
// prototype is in vulkan_core.h - but fails to link. Extension functions have to be fetched at
// runtime with vkGetDeviceProcAddr once the device exists and the extension is enabled.
//
// Device-level pointers also bypass the loader's dispatch trampoline, so they are the fastest way
// to call these. Members intentionally keep their spec names so they're easy to look up.
// -------------------------------------------------------------------------------------------------
struct RayTracingFunctions
{
    // VK_KHR_acceleration_structure
    PFN_vkCreateAccelerationStructureKHR              vkCreateAccelerationStructureKHR              = nullptr;
    PFN_vkDestroyAccelerationStructureKHR             vkDestroyAccelerationStructureKHR             = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR       vkGetAccelerationStructureBuildSizesKHR       = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR           vkCmdBuildAccelerationStructuresKHR           = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR    vkGetAccelerationStructureDeviceAddressKHR    = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr; // compaction size queries
    PFN_vkCmdCopyAccelerationStructureKHR             vkCmdCopyAccelerationStructureKHR             = nullptr; // compaction copies

    // VK_KHR_ray_tracing_pipeline
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR       = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR                    = nullptr;

    // todo: why is this macro? Why not a static helper func instead?
    void Load(VkDevice InDevice)
    {
#define LOAD_DEVICE_FN(Name)                                                          \
    Name = reinterpret_cast<PFN_##Name>(vkGetDeviceProcAddr(InDevice, #Name));        \
    ENSURE(Name != nullptr, "Failed to load " #Name ". Is the extension enabled?");

        LOAD_DEVICE_FN(vkCreateAccelerationStructureKHR);
        LOAD_DEVICE_FN(vkDestroyAccelerationStructureKHR);
        LOAD_DEVICE_FN(vkGetAccelerationStructureBuildSizesKHR);
        LOAD_DEVICE_FN(vkCmdBuildAccelerationStructuresKHR);
        LOAD_DEVICE_FN(vkGetAccelerationStructureDeviceAddressKHR);
        LOAD_DEVICE_FN(vkCmdWriteAccelerationStructuresPropertiesKHR);
        LOAD_DEVICE_FN(vkCmdCopyAccelerationStructureKHR);

        LOAD_DEVICE_FN(vkCreateRayTracingPipelinesKHR);
        LOAD_DEVICE_FN(vkGetRayTracingShaderGroupHandlesKHR);
        LOAD_DEVICE_FN(vkCmdTraceRaysKHR);

#undef LOAD_DEVICE_FN
    }
};
