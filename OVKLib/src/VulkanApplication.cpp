// Native code.
#include "VulkanApplication.h"
#include "Window.h"
#include "Instance.h"
#include "Surface.h"
#include "LogicalDevice.h"
#include "Swapchain.h"
#include "CommandBuffer.h"
#include "Buffer.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include "Framebuffer.h"
#include <chrono>
#include "Model.h"
#include "Camera.h"
#include "Framebuffer.h"

// External includes.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
namespace OVK
{
    void VulkanApplication::Init()
    {
        OnVulkanInit();
        // Create a Window.
        s_Window = std::make_shared<Window>("Vulkan Application", 1280, 720);

        // Create an Instance,
        s_Instance = std::make_shared<Instance>();

        std::vector<PhysicalDevice> devices;
        devices = s_Instance->GetVKPhysicalDevices();

        // Find a suitable PHYSICAL (GPU) device here.
        bool found = false;

        std::cout << "Searching for a DISCREETE graphics card..." << std::endl;
        // First, check for a dedicated graphics card.
        for (auto& device : devices)
        {
            if (device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                s_PhysicalDevice = std::make_shared<PhysicalDevice>(device);
                found = true;
                std::cout << "Found a suitable DISCREETE graphics card. \n\t Card Name ---> " 
                    << s_PhysicalDevice->GetVKProperties().deviceName << std::endl;
                break;
            }
        }
        // If still haven't found a GPU, we pick the first available one without looking at any other condition.
        if (!found)
        {
            std::cout << "Could not find a DISCREETE graphics card.\n" << std::endl;
            std::cout << "Searching for an INTEGRATED graphics card..." << std::endl;
            for (auto& device : devices)
            {
                s_PhysicalDevice = std::make_shared<PhysicalDevice>(device);
                found = true;
                std::cout << "Found a suitable INTEGRATED graphics card. \n\t Card Name ---> " << s_PhysicalDevice->GetVKProperties().deviceName << std::endl;
                break;
            }
        }

        m_MSAA = GetMaxUsableSampleCount(s_PhysicalDevice);

        ASSERT(found, "Could not find a GPU.");

        // Create the surface.
        s_Surface = std::make_shared<Surface>();

        // Sets up a graphics, present and compute queue family and stores their indices for later use.
        SetupQueueFamilies();

        // Logical Device creation.
        s_Device = std::make_shared<LogicalDevice>(m_DeviceExtensions);

        // Swapchain creation.
        s_Swapchain = std::make_shared<Swapchain>();

        s_Camera = std::make_shared<Camera>(m_CamFOV, s_Surface->GetVKExtent().width / (float)s_Surface->GetVKExtent().height, m_CamNearClip, m_CamFarClip);
    }

    void VulkanApplication::Run()
    {
        // Initializes Vulkan here.
        Init();

        // Client OnStart() code is called here. This usually contains pipeline, render pass creations and model loadings.
        OnStart();

        while (!glfwWindowShouldClose(s_Window->GetNativeWindow()))
        {
            // Checks events like button presses.
            glfwPollEvents();

            // Update the time it took to render the previous scene.
            m_Time = glfwGetTime(); 

            m_LastFrameRenderTime = GetRenderTime();

            // Find delta time.
            float deltaTime = DeltaTime();

            // Update camera movement using delta time.
            s_Camera->OnUpdate(deltaTime);

            // Update window events.
            s_Window->OnUpdate();

            // Call client OnUpdate() code here. This usually contains draw calls and other per frame operations.
            OnUpdate();
        }
        vkDeviceWaitIdle(s_Device->GetVKDevice());

        // Call client OnCleanup() code here. This usually contains pointer deletions.
        OnCleanup();
    }

    void VulkanApplication::SetupQueueFamilies()
    {
        // Here, fetch the first queue that supports graphics operations.
        uint64_t index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        s_GraphicsQueueFamily = index;

        // Leaving this part here in case you want to debug the queue families.
        std::vector<VkQueueFamilyProperties> props;
        uint32_t propertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, nullptr);

        props.resize(propertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, props.data());
        // --------------------------------------------------------------------------------------------------------------

        // Check whether the graphics queue we just got also supports present operations.
        if (s_PhysicalDevice->CheckPresentSupport(index, s_Surface->GetVKSurface()))
        {
            s_PresentQueueFamily = s_GraphicsQueueFamily;
        }
        // If the graphics queue doesn't support present operations, we try to find another queue that supports it.
        else
        {
            auto queueFamilies = s_PhysicalDevice->GetQueueFamilies();
            for (const QueueFamily& family : queueFamilies)
            {
                if (s_PhysicalDevice->CheckPresentSupport(family.Index, s_Surface->GetVKSurface()))
                {
                    s_PresentQueueFamily = index;
                    break;
                }
            }
        }
        ASSERT(s_PresentQueueFamily != -1, "Could not find a PRESENT queue.");

        // Try to find a compute queue family.
        index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_COMPUTE_BIT);
        s_ComputeQueueFamily = index;
    }

    VkSampleCountFlagBits VulkanApplication::GetMaxUsableSampleCount(const Ref<PhysicalDevice>& physDevice)
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physDevice->GetVKPhysicalDevice(), &physicalDeviceProperties);

        VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
        if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
        if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
        if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
        if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
        if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
        if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

        return VK_SAMPLE_COUNT_1_BIT;
    }

    VkFormat VulkanApplication::FindSupportedFormat(const Ref<PhysicalDevice>& physDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
    {
        bool found = false;
        for (VkFormat format : candidates)
        {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physDevice->GetVKPhysicalDevice(), format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        ASSERT(found, "Failed to find supported format!");
    }
    float VulkanApplication::DeltaTime()
    {
        float currentFrameRenderTime, deltaTime;
        currentFrameRenderTime = m_Time;
        deltaTime = currentFrameRenderTime - m_LastFrameRenderTime;
        m_LastFrameRenderTime = currentFrameRenderTime;
        return deltaTime;
    }
    float VulkanApplication::GetRenderTime()
    {
        return glfwGetTime();
    }
}