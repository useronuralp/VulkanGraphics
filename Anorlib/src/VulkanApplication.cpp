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
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include "Model.h"
#include "Camera.h"
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
        PhysicalDevice preferredGPU;
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
        uint64_t index = s_PhysicalDevice->FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
        s_GraphicsQueueIndex = index;

        // Check whether the graphics queue we just got also supports present operations.
        if (s_PhysicalDevice->CheckPresentSupport(index, s_Surface->GetVKSurface()))
        {
            s_PresentQueueIndex = s_GraphicsQueueIndex;
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

        // Find the most suitable depth format.
        VkFormat depthFormat = VK_FORMAT_MAX_ENUM;
        std::vector<VkFormat> candidates =
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        };

        m_DepthFormat = FindSupportedFormat(s_PhysicalDevice, candidates, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);


        // Render pass creation. (Should a render pass be customisable / changable in between every draw call ?)
        s_ModelRenderPass = std::make_shared<RenderPass> (m_DepthFormat);

        // Swapchain creation.
        m_Swapchain = std::make_shared<Swapchain> (m_DepthFormat);


        s_Camera = std::make_shared<Camera>(45.0f, s_Surface->GetVKExtent().width / (float)s_Surface->GetVKExtent().height, 0.01f, 100.0f);
        // Create Sponza.
        m_Model = new Anor::Model(
            std::string(SOLUTION_DIR) + "Anorlib\\models\\MaleniaHelmet\\scene.gltf", 
            {
                // You need to specify you shader bindings and their structure here one by one. The order also needs to be same as they appear in your shaders.
                    ShaderBindingSpecs
                    { 
                        // Buffer Index 0. Model Matrix.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::MAT4,
                        ShaderStage::VERTEX
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 1. View Matrix.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::MAT4,
                        ShaderStage::VERTEX
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 2. Proj Matrix.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::MAT4,
                        ShaderStage::VERTEX
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 3. Directional Light location.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::VEC3,
                        ShaderStage::FRAGMENT
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 4. Camera Pos.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::VEC3,
                        ShaderStage::FRAGMENT
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 5. Camera Pos.
                        ShaderBindingType::UNIFORM_BUFFER,
                        UniformUsageType::VEC3,
                        ShaderStage::FRAGMENT
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 6.
                        ShaderBindingType::TEXTURE_SAMPLER,
                        UniformUsageType::DIFFUSE_SAMPLER,
                        ShaderStage::FRAGMENT
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 7.
                        ShaderBindingType::TEXTURE_SAMPLER,
                        UniformUsageType::NORMAL_SAMPLER,
                        ShaderStage::FRAGMENT
                    },
                    ShaderBindingSpecs
                    {
                        // Buffer Index 8.
                        ShaderBindingType::TEXTURE_SAMPLER,
                        UniformUsageType::ROUGHNESS_METALLIC_SAMPLER,
                        ShaderStage::FRAGMENT
                    },
            },
            "shaders/PBRShaderVERT.spv",
            "shaders/PBRShaderFRAG.spv"
        );
        m_Model->Scale(0.7f, 0.7f, 0.7f);

        s_Camera->GetProjectionMatrix();

        glm::mat4 viewMat = s_Camera->GetViewMatrix();
        glm::mat4 projMat = s_Camera->GetProjectionMatrix();

        m_Model->UpdateUniformBuffer(1, &viewMat, sizeof(viewMat));
        m_Model->UpdateUniformBuffer(2, &projMat, sizeof(projMat));

        // A fence that is signalled when rendering is finished.
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_RenderingCompleteSemaphore) == VK_SUCCESS, "Failed to create rendering complete semaphore.");
        ASSERT(vkCreateFence(s_Device->GetVKDevice(), &fenceCreateInfo, nullptr, &m_InRenderingFence) == VK_SUCCESS, "Failed to create is rendering fence.");
        ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphore) == VK_SUCCESS, "Failed to create image available semaphore.");


        m_LightSphere = new Anor::Model(
            std::string(SOLUTION_DIR) + "Anorlib\\models\\Sphere\\scene.gltf",
            {
                // You need to specify you shader bindings and their structure here one by one. The order also needs to be same as they appear in your shaders.
            ShaderBindingSpecs
            {
                // Buffer Index 0. Model Matrix.
                ShaderBindingType::UNIFORM_BUFFER,
                UniformUsageType::MAT4,
                ShaderStage::VERTEX
                },
                ShaderBindingSpecs
                {
                // Buffer Index 1. View Matrix.
                ShaderBindingType::UNIFORM_BUFFER,
                UniformUsageType::MAT4,
                ShaderStage::VERTEX
            },
            ShaderBindingSpecs
            {
                // Buffer Index 2. Proj Matrix.
                ShaderBindingType::UNIFORM_BUFFER,
                UniformUsageType::MAT4,
                ShaderStage::VERTEX
            },
            },
            "shaders/lightSourceVERT.spv",
            "shaders/lightSourceFRAG.spv"
        );
        m_LightSphere->Scale(0.005f, 0.005f, 0.005f);
        //m_LightSphere->Translate(0.0f, 200.0f, 0.0f);

        //const float cubeVertices[288] = {
        //    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
        //     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
        //     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        //     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
        //    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
        //    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
        //
        //    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
        //     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f,
        //     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        //     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f,
        //    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f,
        //    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f,
        //
        //    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        //    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        //    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        //    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        //    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        //    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        //
        //     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        //     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,
        //     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        //     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f,
        //     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f,
        //     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,
        //
        //    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
        //     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f,
        //     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        //     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f,
        //    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f,
        //    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f,
        //
        //    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f,
        //     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f,
        //     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        //     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f,
        //    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f,
        //    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f
        //};
        //std::vector<ShaderBindingSpecs> specs =
        //{
        //    // You need to specify you shader bindings and their structure here one by one. The order also needs to be same as they appear in your shaders.
        //    ShaderBindingSpecs
        //    {
        //    // Buffer Index 0. Model Matrix.
        //    ShaderBindingType::UNIFORM_BUFFER,
        //    UniformUsageType::MAT4,
        //    ShaderStage::VERTEX
        //    },
        //    ShaderBindingSpecs
        //    {
        //        // Buffer Index 1. View Matrix.
        //        ShaderBindingType::UNIFORM_BUFFER,
        //        UniformUsageType::MAT4,
        //        ShaderStage::VERTEX
        //    },
        //    ShaderBindingSpecs
        //    {
        //        // Buffer Index 2. Proj Matrix.
        //        ShaderBindingType::UNIFORM_BUFFER,
        //        UniformUsageType::MAT4,
        //        ShaderStage::VERTEX
        //    },
        //};
        //Ref<DescriptorSet> dscSet = std::make_shared<DescriptorSet>(specs);
        //m_Cube = new Mesh(cubeVertices, (size_t)(sizeof(float) * 8 * 6 * 6), 288, dscSet, "shaders/lightSourceVERT.spv", "shaders/lightSourceFRAG.spv");
    }

    // TO DO: Carry this part into a Renderer class.
    void VulkanApplication::Run()
    {
        glm::vec3 pointLightPosition = glm::vec3(50.0f, 50.0f, 50.0f);
        glm::vec3 directionalLightPosition = glm::vec3(300.0f, 300.0f, 300.0f);

        while (!glfwWindowShouldClose(s_Window->GetNativeWindow()))
        {
            glfwPollEvents(); // Checks for events like button presses.

            // From here on, frame drawing happens. This part could and should be carried into a Renderer class.
            vkWaitForFences(s_Device->GetVKDevice(), 1, &m_InRenderingFence, VK_TRUE, UINT64_MAX);

            // Check if window is resized. Break immidietly if that is the case.
            uint32_t outIimageIndex = -1;
            VkResult rslt = vkAcquireNextImageKHR(s_Device->GetVKDevice(), m_Swapchain->GetVKSwapchain(), UINT64_MAX, m_ImageAvailableSemaphore, VK_NULL_HANDLE, &outIimageIndex);

            if (rslt == VK_ERROR_OUT_OF_DATE_KHR || s_Window->IsWindowResized())
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                m_Swapchain->OnResize();
                m_Model->OnResize();
                m_LightSphere->OnResize();
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


            // Record Command Buffer. You can do multiple object drawing here.
            VkCommandPool pool;
            VkCommandBuffer cmdBuffer;
            CommandBuffer::Create(s_GraphicsQueueIndex, pool, cmdBuffer);
            CommandBuffer::Begin(cmdBuffer);
            CommandBuffer::BeginRenderPass(cmdBuffer, s_ModelRenderPass, m_Swapchain->GetFramebuffers()[outIimageIndex]);

            // Drawing individual models !!STARTS!! here
            // Adjust the model matrix of Sponza
            pointLightPosition = glm::vec3(m_LightSphere->GetModelMatrix()[3][0], m_LightSphere->GetModelMatrix()[3][1], m_LightSphere->GetModelMatrix()[3][2]);

            m_Model->UpdateUniformBuffer(3, &directionalLightPosition, sizeof(directionalLightPosition));
            glm::vec3 cameraPos = s_Camera->GetPosition();
            m_Model->UpdateUniformBuffer(4, &cameraPos, sizeof(cameraPos));
            m_Model->UpdateUniformBuffer(5, &pointLightPosition, sizeof(pointLightPosition));
            glm::mat4 view = s_Camera->GetViewMatrix();
            glm::mat4 proj = s_Camera->GetProjectionMatrix();
            m_Model->UpdateUniformBuffer(1, &view, sizeof(view));
            m_Model->UpdateUniformBuffer(2, &proj, sizeof(proj));
            //m_Model->Rotate(glm::radians(500.0f) * deltaTime, 0.0f, 0.0f, 1.0f);
            //m_Model->Translate(1.0f * deltaTime, 0.0f, 0.0f);
            m_Model->DrawIndexed(cmdBuffer); // Call low level draw calls in here.

            //glm::mat4 modelMat = m_Cube->GetModelMatrix();
            //modelMat[3][0] = cameraPos.x;
            //modelMat[3][1] = cameraPos.y + 0.05f;
            //modelMat[3][2] = cameraPos.z;
            //m_Cube->UpdateUniformBuffer(0, &modelMat, sizeof(modelMat));

            m_LightSphere->UpdateUniformBuffer(1, &view, sizeof(view));
            m_LightSphere->UpdateUniformBuffer(2, &proj, sizeof(proj));
            m_LightSphere->Translate(0.0f, 0.0f, 10.0f * deltaTime);
            m_LightSphere->DrawIndexed(cmdBuffer);
            // Drawing individual models !!ENDS!! here

            CommandBuffer::EndRenderPass(cmdBuffer);
            CommandBuffer::End(cmdBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

            VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphore };
            VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = waitSemaphores;
            submitInfo.pWaitDstStageMask = waitStages;

            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &cmdBuffer;

            VkSemaphore signalSemaphores[] = { m_RenderingCompleteSemaphore };
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = signalSemaphores;

            VkQueue graphicsQueue = s_Device->GetGraphicsQueue();

            // As soon as you call the following function, Vulkan starts rendering onto swapchain images.
            ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_InRenderingFence) == VK_SUCCESS, "Failed to submit draw command buffer!");

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = signalSemaphores;

            VkSwapchainKHR swapChains[] = { m_Swapchain->GetVKSwapchain() };
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = swapChains;
            presentInfo.pImageIndices = &outIimageIndex;
            presentInfo.pResults = nullptr; // Optional

            VkQueue presentQueue = s_Device->GetPresentQueue();
            VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);


            if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            {
                vkDeviceWaitIdle(s_Device->GetVKDevice());
                m_Swapchain->OnResize();
                m_Model->OnResize();
                m_LightSphere->OnResize();
                s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
                continue;
            }
            ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");
            CommandBuffer::FreeCommandBuffer(cmdBuffer, pool);
        }
        vkDeviceWaitIdle(s_Device->GetVKDevice());
        vkDestroySemaphore(s_Device->GetVKDevice(), m_ImageAvailableSemaphore, nullptr);
        vkDestroySemaphore(s_Device->GetVKDevice(), m_RenderingCompleteSemaphore, nullptr);
        vkDestroyFence(s_Device->GetVKDevice(), m_InRenderingFence, nullptr);
        delete m_Model;
        delete m_LightSphere;
    }
    float VulkanApplication::DeltaTime()
    {
        float currentFrameRenderTime, deltaTime;
        currentFrameRenderTime = m_Time;
        deltaTime = currentFrameRenderTime - m_LastFrameRenderTime;
        m_LastFrameRenderTime = currentFrameRenderTime;
        return deltaTime;
    }
}