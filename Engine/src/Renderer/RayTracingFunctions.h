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
//
// Ownership: VulkanPathTracer owns the one instance and loads it once in Init(). Anything below it
// that issues raw RT commands (AS builders, RT pipeline / SBT) gets a `const RayTracingFunctions&`
// passed in explicitly, so the dependency shows up in signatures and nothing outside the path
// tracer can reach these. vkGetDeviceProcAddr is a by-name lookup meant for init time, not for
// call sites (vkCmdTraceRaysKHR runs every frame).
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

    void Load(VkDevice InDevice)
    {
        // Macro rather than a function: #Name and PFN_##Name are both derived from the one token, so the
        // string you look up and the pointer type you cast to can't drift apart. Loading "vkCmdTraceRaysKHR"
        // into a PFN of another signature would compile fine and crash at the first call.
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
