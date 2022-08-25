// Native code.
#include "VulkanApplication.h"
#include "Window.h"
#include "Instance.h"
#include "Surface.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
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
namespace Anor
{
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
    VulkanApplication::VulkanApplication()
    {
        // Create a Window.
        s_Window = std::make_shared<Window>("Vulkan Application", 1280, 720);

        // Create an Instance,
        s_Instance = std::make_shared<Instance>();

        std::vector<PhysicalDevice> devices;
        devices = s_Instance->GetVKPhysicalDevices();

        // Find a suitable device here.
        bool found = false;

        std::cout << "Searching for a DISCREETE graphics card..." << std::endl;
        // First, check for a dedicated graphics card.
        for (auto& device : devices)
        {
            if (device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                s_PhysicalDevice = std::make_shared<PhysicalDevice>(device);
                found = true;
                std::cout << "Found a suitable DISCREETE graphics card. \n\t Card Name ---> " << s_PhysicalDevice->GetVKProperties().deviceName << std::endl;
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

        ASSERT(found, "Could not find a GPU.");

        // Create the surface.
        s_Surface = std::make_shared<Surface>();

        // Here, fetch the first queue that supports graphics operations.
        uint64_t index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT || VK_QUEUE_COMPUTE_BIT);
        s_GraphicsANDComputeQueueIndex = index;

        // Check whether the graphics queue we just got also supports present operations.
        if (s_PhysicalDevice->CheckPresentSupport(index, s_Surface->GetVKSurface()))
        {
            s_PresentQueueIndex = s_GraphicsANDComputeQueueIndex;
        }
        // If the graphics queue doesn't support present operations, we try to find another queue that supports it.
        else
        {
            auto queueFamilies = s_PhysicalDevice->GetQueueFamilies();
            for (const QueueFamily& family : queueFamilies)
            {
                if (s_PhysicalDevice->CheckPresentSupport(family.Index, s_Surface->GetVKSurface()))
                {
                    s_PresentQueueIndex = index;
                    break;
                }
            }
        }
        ASSERT(s_PresentQueueIndex != -1, "Could not find a PRESENT queue.");

        // Logical Device creation.
        s_Device = std::make_shared<LogicalDevice>();

        // Swapchain creation.
        s_Swapchain = std::make_shared<Swapchain>();

        s_Camera = std::make_shared<Camera>(45.0f, s_Surface->GetVKExtent().width / (float)s_Surface->GetVKExtent().height, 0.1f, 1500.0f);

        // A fence that is signalled when rendering is finished.
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_RenderingCompleteSemaphore) == VK_SUCCESS, "Failed to create rendering complete semaphore.");
        ASSERT(vkCreateFence(s_Device->GetVKDevice(), &fenceCreateInfo, nullptr, &m_InRenderingFence) == VK_SUCCESS, "Failed to create is rendering fence.");
        ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) == VK_SUCCESS, "Failed to create image available semaphore.");
    }

    // TO DO: Carry this part into a Renderer class.
    void VulkanApplication::Run()
    {
        // Client OnStart() code is called here. This usually contains initializations and model loadings.
        OnStart();

        while (!glfwWindowShouldClose(s_Window->GetNativeWindow()))
        {
            // Checks for events like button presses.
            glfwPollEvents(); 

            // From here on, frame drawing happens. This part could and should be carried into a Renderer class.
            vkWaitForFences(s_Device->GetVKDevice(), 1, &m_InRenderingFence, VK_TRUE, UINT64_MAX);

            // Check if window is resized. Break immidietly if that is the case.
            m_OutImageIndex = -1;
            VkResult rslt = vkAcquireNextImageKHR(s_Device->GetVKDevice(), s_Swapchain->GetVKSwapchain(), UINT64_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &m_OutImageIndex);
            s_Swapchain->SetActiveImageIndex(m_OutImageIndex);


            if (rslt == VK_ERROR_OUT_OF_DATE_KHR || s_Window->IsWindowResized())
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                s_Swapchain->OnResize();
                OnWindowResize();
                s_Window->OnResize();
                s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
                continue;
            }
            ASSERT(rslt == VK_SUCCESS, "Failed to acquire next image.");


            m_Time = glfwGetTime(); // Update the time it took to render the previous scene.
            float deltaTime = DeltaTime();

            s_Camera->OnUpdate(deltaTime);
            s_Window->OnUpdate();

            vkResetFences(s_Device->GetVKDevice(), 1, &m_InRenderingFence);

            //Create a temporary pool and a command buffer every frame. This doesn't cost too much performance, we can keep it like this for now.
            VkCommandPool   pool;
            VkCommandBuffer cmdBuffer;

            CommandBuffer::Create(s_GraphicsANDComputeQueueIndex, pool, cmdBuffer);
            CommandBuffer::Begin(cmdBuffer);

            m_ActiveCommandBuffer = cmdBuffer;
            m_ActiveCommandPool = pool;

            // Client OnUpdate code is called here. This function will usually contain draw calls & functions with "vkCmd" prefix.
            OnUpdate();

            CommandBuffer::End(m_ActiveCommandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            VkSemaphore waitSemaphores[]      = { m_ImageAvailableSemaphore };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount     = 1;
            submitInfo.pWaitSemaphores        = waitSemaphores;
            submitInfo.pWaitDstStageMask      = waitStages;
            submitInfo.commandBufferCount     = 1;
            submitInfo.pCommandBuffers        = &m_ActiveCommandBuffer;
            VkSemaphore signalSemaphores[]    = { m_RenderingCompleteSemaphore };
            submitInfo.signalSemaphoreCount   = 1;
            submitInfo.pSignalSemaphores      = signalSemaphores;
            VkQueue graphicsQueue             = s_Device->GetGraphicsQueue();

            // As soon as you call the following function, Vulkan starts rendering onto swapchain images.
            ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_InRenderingFence) == VK_SUCCESS, "Failed to submit draw command buffer!");

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType               = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount  = 1;
            presentInfo.pWaitSemaphores     = signalSemaphores;
            VkSwapchainKHR swapChains[]     = { s_Swapchain->GetVKSwapchain() };
            presentInfo.swapchainCount      = 1;
            presentInfo.pSwapchains         = swapChains;
            presentInfo.pImageIndices       = &m_OutImageIndex;
            presentInfo.pResults            = nullptr; // Optional

            VkQueue presentQueue = s_Device->GetPresentQueue();
            VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

            if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                s_Swapchain->OnResize();
                OnWindowResize();
                s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
                continue;
            }
            ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");

            CommandBuffer::FreeCommandBuffer(m_ActiveCommandBuffer, m_ActiveCommandPool);

        }
        vkDeviceWaitIdle(s_Device->GetVKDevice());
        vkDestroySemaphore(s_Device->GetVKDevice(), m_ImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(s_Device->GetVKDevice(), m_RenderingCompleteSemaphore, nullptr);
        vkDestroyFence(s_Device->GetVKDevice(), m_InRenderingFence, nullptr);

        OnCleanup();
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
    glm::mat4 VulkanApplication::GetCameraViewMatrix()
    {
        return s_Camera->GetViewMatrix();
    }
    glm::mat4 VulkanApplication::GetCameraProjectionMatrix()
    {
        return s_Camera->GetProjectionMatrix();
    }
    glm::vec3 VulkanApplication::GetCameraPosition()
    {
        return s_Camera->GetPosition();
    }
    void VulkanApplication::BeginRenderPass()
    {
        CommandBuffer::BeginRenderPass(m_ActiveCommandBuffer, s_Swapchain->GetSwapchainRenderPass(), s_Swapchain->GetFramebuffers()[m_OutImageIndex]);
    }
    void VulkanApplication::BeginRenderPass(const VkCommandBuffer& cmdBuffer, const VkRenderingInfoKHR& renderingInfo)
    {
        CommandBuffer::BeginRenderPass(cmdBuffer, renderingInfo);
    }
    void VulkanApplication::BeginCustomRenderPass(const VkRenderPassBeginInfo& renderPassBeginInfo)
    {
        CommandBuffer::BeginCustomRenderPass(m_ActiveCommandBuffer, renderPassBeginInfo);
    }
    void VulkanApplication::EndDepthPass()
    {
        CommandBuffer::EndRenderPass(m_ActiveCommandBuffer);
    }
    void VulkanApplication::SetViewport(const VkViewport& viewport)
    {
        CommandBuffer::SetViewport(m_ActiveCommandBuffer, viewport);
    }
    void VulkanApplication::SetScissor(const VkRect2D& scissor)
    {
        CommandBuffer::SetScissor(m_ActiveCommandBuffer, scissor);
    }
    void VulkanApplication::SetDepthBias(float depthBiasConstant, float slopeBias)
    {
        CommandBuffer::SetDepthBias(m_ActiveCommandBuffer, depthBiasConstant, slopeBias);
    }
    void VulkanApplication::EndRenderPass()
    {
        CommandBuffer::EndRenderPass(m_ActiveCommandBuffer);
    }
    void VulkanApplication::EndRendering()
    {
        CommandBuffer::EndRendering(m_ActiveCommandBuffer);
    }
    VkDevice VulkanApplication::GetVKDevice()
    {
        return s_Device->GetVKDevice();
    }
}