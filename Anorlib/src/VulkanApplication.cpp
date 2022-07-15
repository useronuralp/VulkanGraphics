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
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
namespace Anor
{
    struct UniformBufferObject
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

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
        m_Window = std::make_shared<Window>("Vulkan Application", 1280, 720);

        // Create an Instance,
        m_Instance = std::make_shared<Instance>();

        std::vector<PhysicalDevice> devices;
        devices = m_Instance->GetVKPhysicalDevices();

        // Find a suitable device here.
        PhysicalDevice preferredGPU;
        bool found = false;

        std::cout << "Searching for a DISCREETE graphics card..." << std::endl;
        // First, check for a dedicated graphics card.
        for (auto& device : devices)
        {
            if (device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                m_PhysicalDevice = std::make_shared<PhysicalDevice>(device);
                found = true;
                std::cout << "Found a suitable DISCREETE graphics card. \n\t Card Name ---> " << m_PhysicalDevice->GetVKProperties().deviceName << std::endl;
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
                m_PhysicalDevice = std::make_shared<PhysicalDevice>(device);
                found = true;
                std::cout << "Found a suitable INTEGRATED graphics card. \n\t Card Name ---> " << m_PhysicalDevice->GetVKProperties().deviceName << std::endl;
                break;
            }
        }

        ASSERT(found, "Could not find a GPU.");

        // Create the surface.
        m_Surface = std::make_shared<Surface>(m_Instance, m_Window, m_PhysicalDevice);

        // Here, fetch the first queue that supports graphics operations.
        uint64_t index = m_PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        m_GraphicsQueueIndex = index;

        // Check whether the graphics queue we just got also supports present operations.
        if (m_PhysicalDevice->CheckPresentSupport(index, m_Surface->GetVKSurface()))
        {
            m_PresentQueueIndex = m_GraphicsQueueIndex;
        }
        // If the graphics queue doesn't support present operations, we try to find another queue that supports it.
        else
        {
            auto queueFamilies = m_PhysicalDevice->GetQueueFamilies();
            for (const QueueFamily& family : queueFamilies)
            {
                if (m_PhysicalDevice->CheckPresentSupport(family.Index, m_Surface->GetVKSurface()))
                {
                    m_PresentQueueIndex = index;
                    break;
                }
            }
        }
        ASSERT(m_PresentQueueIndex != -1, "Could not find a PRESENT queue.");

        // Logical Device creation.
        m_Device = std::make_shared<LogicalDevice>(m_PhysicalDevice, m_Surface, m_Window, m_GraphicsQueueIndex, m_PresentQueueIndex);


        // Find the most suitable depth format.
        VkFormat depthFormat = VK_FORMAT_MAX_ENUM;
        std::vector<VkFormat> candidates =
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        depthFormat = FindSupportedFormat(m_PhysicalDevice, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);

        // Render pass creation.
        m_RenderPass = std::make_shared<RenderPass> (m_Device, m_Surface->GetVKSurfaceFormat().format, depthFormat);

        // Swapchain creation.
        m_Swapchain = std::make_shared<Swapchain> (m_PhysicalDevice, m_Device, m_Surface, m_RenderPass, depthFormat);

        // Descriptor Set creaation.
        m_DescriptorSet = std::make_shared<DescriptorSet> (m_Device);

        // Command Buffer creation.
        m_CommandBuffer = std::make_shared<CommandBuffer> (m_Device, m_GraphicsQueueIndex, m_DescriptorSet);

        // Pipeline creation.
        m_Pipeline = std::make_shared<Pipeline>(m_Device, m_Swapchain, m_RenderPass, m_DescriptorSet);


        // This part should be taken care of by a "Model" class.
        std::vector<Anor::Vertex> vertices =
        {
            {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, -0.5f , 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, 0.5f  , 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            {{-0.5f, 0.5f , 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
        
            {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
            {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
            {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f},  {1.0f, 1.0f}},
            {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
        };
        
        std::vector<uint32_t> indices =
        {
            4, 5, 6, 6, 7, 4,
            0, 1, 2, 2, 3, 0,
        };
        
        std::string tst = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\texture.png").c_str();
        m_VBO = std::make_shared<VertexBuffer>(m_Device, m_PhysicalDevice, vertices, m_GraphicsQueueIndex);
        m_IBO = std::make_shared<IndexBuffer>(m_Device, m_PhysicalDevice, indices, m_GraphicsQueueIndex);
        m_UBO = std::make_shared<UniformBuffer>(m_Device, m_PhysicalDevice, sizeof(UniformBufferObject));
        m_ImBO  = std::make_shared<ImageBuffer>(m_Device, m_PhysicalDevice, tst.c_str(), m_GraphicsQueueIndex);
        

        m_UBO->UpdateUniformBuffer(sizeof(UniformBufferObject), m_DescriptorSet);
        m_ImBO->UpdateImageBuffer(m_DescriptorSet);

        Run();
    }

    // TO DO: Carry this part into a Renderer class.
    void VulkanApplication::Run()
    {
        while (!glfwWindowShouldClose(m_Window->GetNativeWindow()))
        {
            glfwPollEvents(); // Checks for events like button presses.

            // From here on, frame drawing happens. This part could and should be carried into a Renderer class.
            VkFence waitFence = m_Swapchain->GetIsRenderingFence();
            vkWaitForFences(m_Device->GetVKDevice(), 1, &waitFence, VK_TRUE, UINT64_MAX);
            m_Swapchain->ResetFence();

            uint32_t imageIndex = m_Swapchain->AcquireNextImage();

            m_CommandBuffer->ResetCommandBuffer();
            m_CommandBuffer->RecordDrawingCommandBuffer(imageIndex, m_RenderPass, m_Swapchain, m_Pipeline, m_Swapchain->GetFramebuffers()[imageIndex], m_VBO, m_IBO);


            // Update uniform buffer.-------------------------
            static auto startTime = std::chrono::high_resolution_clock::now();
            
            auto currentTime = std::chrono::high_resolution_clock::now();
            float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
            
            UniformBufferObject ubo{};
            ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
            ubo.proj = glm::perspective(glm::radians(45.0f), m_Swapchain->GetExtent().width / (float)m_Swapchain->GetExtent().height, 0.1f, 10.0f);
            ubo.proj[1][1] *= -1;
            
            // Map UBO memory and write to it every frame.
            void* data;
            vkMapMemory(m_Device->GetVKDevice(), m_UBO->GetBufferMemory(), 0, sizeof(ubo), 0, &data);
            memcpy(data, &ubo, sizeof(ubo));
            vkUnmapMemory(m_Device->GetVKDevice(), m_UBO->GetBufferMemory());
            // End of update uniform buffers------------------

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { m_Swapchain->GetImageAvailableSemaphore() };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            VkCommandBuffer CB = m_CommandBuffer->GetVKCommandBuffer();
            submitInfo.pCommandBuffers = &CB;

            VkSemaphore signalSemaphores[] = { m_Swapchain->GetRenderingCompleteSemaphore() };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            VkQueue graphicsQueue = m_Device->GetGraphicsQueue();

            // As soon as you call the following function, Vulkan starts rendering onto swapchain images.
            ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFence) == VK_SUCCESS, "Failed to submit draw command buffer!");

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { m_Swapchain->GetVKSwapchain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &imageIndex;
            presentInfo.pResults = nullptr; // Optional

            VkQueue presentQueue = m_Device->GetPresentQueue();
            VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo); // graphics and present queues are the same in my computer.

            ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
        }
        vkDeviceWaitIdle(m_Device->GetVKDevice());
    }
}