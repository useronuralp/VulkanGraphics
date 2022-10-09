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
    uint32_t VulkanApplication::m_ActiveImageIndex = -1; // Definde the static variable here.

    VulkanApplication::VulkanApplication(uint32_t framesInFlight)
        : m_FramesInFlight(framesInFlight)
    {
        
    }

    VulkanApplication::~VulkanApplication()
    {
        for (int i = 0; i < m_FramesInFlight; i++)
        {
            vkDestroySemaphore(s_Device->GetVKDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(s_Device->GetVKDevice(), m_RenderingCompleteSemaphores[i], nullptr);
            vkDestroyFence(s_Device->GetVKDevice(), m_InFlightFences[i], nullptr);
        }
    }

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

        // Initialize 2 sempahores and a single fence needed to synchronize rendering and presentation.
        m_RenderingCompleteSemaphores.resize(m_FramesInFlight);
        m_ImageAvailableSemaphores.resize(m_FramesInFlight);
        m_InFlightFences.resize(m_FramesInFlight);

        // Setup the fences and semaphores needed to synchronize the rendering.
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        // Create the scynhronization objects as many times as the frames in flight number.
        for (int i = 0; i < m_FramesInFlight; i++)
        {
            ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_RenderingCompleteSemaphores[i]) == VK_SUCCESS, "Failed to create rendering complete semaphore.");
            ASSERT(vkCreateFence(s_Device->GetVKDevice(), &fenceCreateInfo, nullptr, &m_InFlightFences[i]) == VK_SUCCESS, "Failed to create is rendering fence.");
            ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) == VK_SUCCESS, "Failed to create image available semaphore.");
        }
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

            vkWaitForFences(s_Device->GetVKDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

            VkResult result = vkAcquireNextImageKHR(s_Device->GetVKDevice(), s_Swapchain->GetVKSwapchain(), UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_ActiveImageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || s_Window->IsWindowResized())
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                s_Swapchain->OnResize();
                OnWindowResize();
                s_Window->OnResize();
                s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
                continue;
            }

            ASSERT(result == VK_SUCCESS, "Failed to acquire next image.");

            vkResetFences(s_Device->GetVKDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

            // Call client OnUpdate() code here. This function usually contains command buffer calls and a SubmitCommandBuffer() call.
            OnUpdate();

            // Submit the command buffer that we got frome the OnUpdate() function.
            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = m_CommandBufferReference;
            VkSemaphore signalSemaphores[] = { m_RenderingCompleteSemaphores[m_CurrentFrame] };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            VkQueue graphicsQueue = s_Device->GetGraphicsQueue();
            ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) == VK_SUCCESS, "Failed to submit draw command buffer!");

            // Present the drawn image to the swapchain when the drawing is completed. This check is done via a semaphore.
            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;
            VkSwapchainKHR swapChains[] = { s_Swapchain->GetVKSwapchain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &m_ActiveImageIndex;
            presentInfo.pResults = nullptr; // Optional

            result = vkQueuePresentKHR(graphicsQueue, &presentInfo);

            // Check if window has been resized.
            if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                s_Swapchain->OnResize();
                OnWindowResize();
                s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
                continue;
            }
            ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
            CommandBuffer::Reset(*m_CommandBufferReference);

            m_CurrentFrame = ++m_CurrentFrame % m_FramesInFlight;
        }
        vkDeviceWaitIdle(s_Device->GetVKDevice());

        // Call client OnCleanup() code here. This usually contains pointer deletions.
        OnCleanup();
    }

    void VulkanApplication::SubmitCommandBuffer(VkCommandBuffer& cmdBuffer)
    {
        m_CommandBufferReference = &cmdBuffer;
    }

    void VulkanApplication::SetupQueueFamilies()
    {
        // Here, fetch the first queue family that supports graphics operations.
        uint64_t index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        s_GraphicsQueueFamily = index;

        // Check whether the graphics queue we just got also supports present operations.
        ASSERT(s_PhysicalDevice->CheckPresentSupport(s_GraphicsQueueFamily, s_Surface->GetVKSurface()), "Present operations are not supported by the graphics queue. Might want to search for it manually.");

        // Leaving this part here in case you want to debug the queue families.
        std::vector<VkQueueFamilyProperties> props;
        uint32_t propertyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, nullptr);

        props.resize(propertyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(s_PhysicalDevice->GetVKPhysicalDevice(), &propertyCount, props.data());
        // --------------------------------------------------------------------------------------------------------------


        //// If the graphics queue doesn't support present operations, we try to find another queue that supports it.
        //else
        //{
        //    auto queueFamilies = s_PhysicalDevice->GetQueueFamilies();
        //    for (const QueueFamily& family : queueFamilies)
        //    {
        //        if (s_PhysicalDevice->CheckPresentSupport(family.Index, s_Surface->GetVKSurface()))
        //        {
        //            s_PresentQueueFamily = index;
        //            break;
        //        }
        //    }
        //}
        //ASSERT(s_PresentQueueFamily != -1, "Could not find a PRESENT queue.");

        // Try to find a compute queue family.
        index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_COMPUTE_BIT);
        s_ComputeQueueFamily = index;

        // Try to find a transfer queue family.
        index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_TRANSFER_BIT);
        s_TransferQueueFamily = index;
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