#include <string>
#include <chrono>
// ----------------------------
#include "Instance.h"
#include "Window.h"
#include "PhysicalDevice.h"
#include "LogicalDevice.h"
#include "Swapchain.h"
#include "Surface.h"
#include "RenderPass.h"
#include "Pipeline.h"
#include "Framebuffer.h"
#include "CommandPool.h"
#include "CommandBuffer.h"
#include "Buffer.h"
#include "core.h"
#include "DescriptorSet.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
// ----------------------------
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main()
{
	std::vector<const char*> deviceExtensions =
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};

	Anor::Window window("Vulkan Demo", 1280, 720);

	Anor::Instance::CreateInfo createInfo{};
	createInfo.EnableValidation = true;
	createInfo.ExtensionNames = {};
	createInfo.Flags = 0;
	createInfo.AppInfo = {};
	createInfo.pVkAllocator = nullptr;

	Anor::Instance instance(createInfo);

	auto devices = instance.GetVKPhysicalDevices();

	// Find a suitable device here.
	Anor::PhysicalDevice GPU;
	uint32_t GraphicsQueueIndex = -1;
	bool found = false;
	for (auto& device : devices)
	{
		uint32_t index = device.FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
		if (index == Anor::PhysicalDevice::InvalidQueueFamilyIndex)
		{
			std::cerr << "Could not find requested queue families on the card named: " + device.GetPhysicalDeviceName() << std::endl;
		}
		else
		{
			if (device.GetVKProperties().deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			{
				GraphicsQueueIndex = index;
				GPU = device;
				found = true;
			}
			std::cout << "QUEUE INDEX:" + std::to_string(index) << std::endl;
		}
	}
	if (!found)
	{
		for (auto& device : devices)
		{
			uint32_t index = device.FindQueueFamily(VK_QUEUE_GRAPHICS_BIT);
			if (index != Anor::PhysicalDevice::InvalidQueueFamilyIndex)
			{
				GraphicsQueueIndex = index;
				GPU = device;
				found = true;
			}
		}
	}
	if (!found)
	{
		std::cerr << "Could not find a discreete GPU on your system. You could always use an integrated one if you want but that will require modifying of this part of the code." << std::endl;
		__debugbreak();
	}
	
	GPU.SetDeviceExtensions(deviceExtensions);

	// Surface creation.
	Anor::Surface::CreateInfo surfaceCreateInfo{};
	surfaceCreateInfo.pInstance = &instance;
	surfaceCreateInfo.pPhysicalDevice = &GPU;
	surfaceCreateInfo.pWindow = &window;
	Anor::Surface srfc(surfaceCreateInfo);

	// Logical Device creation.
	Anor::LogicalDevice::QueueCreateInfo queueCreateInfo{};
	float priority = 1.0f;
	const float* queuePriorities = &priority;
	queueCreateInfo.QueueFamilyIndex = GraphicsQueueIndex;
	queueCreateInfo.QueueCount = 1;
	queueCreateInfo.pQueuePriorities = queuePriorities;
	queueCreateInfo.Flags = 0;
	queueCreateInfo.SearchForPresentSupport = true;
	queueCreateInfo.QueueType = VK_QUEUE_GRAPHICS_BIT; // TO DO: Add transfer bit here as well to test.
	std::vector<Anor::LogicalDevice::QueueCreateInfo> queueCreateInfos;
	queueCreateInfos.push_back(queueCreateInfo);
	Anor::LogicalDevice::CreateInfo logicalDeviceCreateInfo{};
	logicalDeviceCreateInfo.pEnabledFeatures = {};
	logicalDeviceCreateInfo.ppQueueCreateInfos = queueCreateInfos;
	logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	logicalDeviceCreateInfo.Flags = 0;
	logicalDeviceCreateInfo.pEnabledLayerNames = {};
	logicalDeviceCreateInfo.pPhysicalDevice = &GPU;
	logicalDeviceCreateInfo.pWindow = &window;
	logicalDeviceCreateInfo.pSurface = &srfc;
	Anor::LogicalDevice logicalDevice(logicalDeviceCreateInfo, queueCreateInfos);

	// Render pass createion. TO DO: Let the user choose how many attachments to create and their types.
	Anor::RenderPass::CreateInfo renderPassCreateInfo{};
	renderPassCreateInfo.pLogicalDevice = &logicalDevice;
	renderPassCreateInfo.Format = srfc.GetVKSurfaceFormat().format;
	Anor::RenderPass renderPass(renderPassCreateInfo);

	// Swapchain creation.
	Anor::Swapchain::CreateInfo swapChainCreateInfo{};
	swapChainCreateInfo.pLogicalDevice = &logicalDevice;
	swapChainCreateInfo.pPhysicalDevice = &GPU;
	swapChainCreateInfo.pSurface= &srfc;
	swapChainCreateInfo.pRenderPass = &renderPass;
	Anor::Swapchain swapchain(swapChainCreateInfo);

	// Command Pool creation.
	Anor::CommandPool::CreateInfo cmdPoolCreateInfo{};
	cmdPoolCreateInfo.pLogicalDevice = &logicalDevice;
	cmdPoolCreateInfo.QueueFamilyIndex = GraphicsQueueIndex; // The queue family where the command buffers will be fetched from.
	Anor::CommandPool commandPool(cmdPoolCreateInfo);

	std::vector<Anor::Vertex> vertices =
	{
		{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, -0.5f , 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, 0.5f  , 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
		{{-0.5f, 0.5f , 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

		{{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
		{{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
		{{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
		{{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
	};

	std::vector<uint16_t> indices =
	{
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

	// Vertex Buffer creation.
	Anor::VertexBuffer::CreateInfo VBCreateInfo{};
	VBCreateInfo.pCommandPool = &commandPool;
	VBCreateInfo.pLogicalDevice = &logicalDevice;
	VBCreateInfo.pPhysicalDevice = &GPU;
	VBCreateInfo.pVertices = &vertices;
	Anor::VertexBuffer vertexBuffer(VBCreateInfo);

	// Index Buffer creation.
	Anor::IndexBuffer::CreateInfo IBCreateInfo{};
	IBCreateInfo.pCommandPool = &commandPool;
	IBCreateInfo.pLogicalDevice = &logicalDevice;
	IBCreateInfo.pPhysicalDevice = &GPU;
	IBCreateInfo.pIndices = &indices;
	Anor::IndexBuffer indexBuffer(IBCreateInfo);

	// Uniform Buffer creation.
	Anor::UniformBuffer::CreateInfo UBOCreateInfo{};
	UBOCreateInfo.pLogicalDevice = &logicalDevice;
	UBOCreateInfo.pPhysicalDevice = &GPU;
	UBOCreateInfo.AllocateSize = sizeof(UniformBufferObject);
	Anor::UniformBuffer uniformBuffer(UBOCreateInfo);

	// Descriptor Set creation. Pool and Layout creations are included inside.
	Anor::DescriptorSet::CreateInfo descriptorSetCreateInfo{};
	descriptorSetCreateInfo.pLogicalDevice = &logicalDevice;
	Anor::DescriptorSet descriptorSet(descriptorSetCreateInfo);

	// Graphics Pipeline creation. TO DO: Expose more variables here.
	Anor::Pipeline::CreateInfo graphicsPipelineCreateInfo{};
	graphicsPipelineCreateInfo.pLogicalDevice = &logicalDevice;
	graphicsPipelineCreateInfo.pRenderPass = &renderPass;
	graphicsPipelineCreateInfo.pSwapchain = &swapchain;
	graphicsPipelineCreateInfo.pDescriptorSet = &descriptorSet;
	Anor::Pipeline pipeline(graphicsPipelineCreateInfo);

	// Image Buffer creation.
	Anor::ImageBuffer::CreateInfo imageBufferCreateInfo{};
	imageBufferCreateInfo.pLogicalDevice = &logicalDevice;
	imageBufferCreateInfo.pPhysicalDevice = &GPU;
	imageBufferCreateInfo.pCommandPool = &commandPool;
	imageBufferCreateInfo.ImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	imageBufferCreateInfo.Tiling = VK_IMAGE_TILING_OPTIMAL;
	imageBufferCreateInfo.Usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageBufferCreateInfo.IncludeAlpha = true;
	imageBufferCreateInfo.AnisotropyFilter = GPU.GetVKProperties().limits.maxSamplerAnisotropy;
	imageBufferCreateInfo.TextureMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	std::string tst = (std::string(SOLUTION_DIR) + "Anorlib/textures/texture.png").c_str();
	imageBufferCreateInfo.pTexturePath = tst.c_str();
	Anor::ImageBuffer imageBuffer(imageBufferCreateInfo);


	uniformBuffer.UpdateUniformBuffer(sizeof(UniformBufferObject), descriptorSet.GetVKDescriptorSet()); // Write to the uniform buffer here.
	imageBuffer.UpdateImageBuffer(descriptorSet.GetVKDescriptorSet());
	// TO DO: Split the creation of a command buffer in between CommandBuffer::Start & CommandBuffer::End.
	Anor::CommandBuffer commandBuffer(&commandPool, &logicalDevice, &descriptorSet);


	while (!glfwWindowShouldClose(window.GetNativeWindow()))
	{
		glfwPollEvents(); // Checks for events like button presses.

		// From here on, frame drawing happens. This part could and should be carried into a Renderer class.
		VkFence waitFence = swapchain.GetIsRenderingFence();
		vkWaitForFences(logicalDevice.GetVKDevice(), 1, &waitFence, VK_TRUE, UINT64_MAX);
		swapchain.ResetFence();

		uint32_t imageIndex = swapchain.AcquireNextImage();

		commandBuffer.ResetCommandBuffer();
		commandBuffer.RecordDrawingCommandBuffer(imageIndex, renderPass, swapchain, pipeline, *swapchain.GetFramebuffers()[imageIndex], vertexBuffer, indexBuffer);


		// Update uniform buffer.-------------------------
		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{};
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		ubo.proj = glm::perspective(glm::radians(45.0f), swapchain.GetExtent().width / (float)swapchain.GetExtent().height, 0.1f, 10.0f);
		ubo.proj[1][1] *= -1;

		// Map UBO memory and write to it every frame.
		void* data;
		vkMapMemory(logicalDevice.GetVKDevice(), uniformBuffer.GetBufferMemory(), 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(logicalDevice.GetVKDevice(), uniformBuffer.GetBufferMemory());
		// End of update uniform buffers------------------

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { swapchain.GetImageAvailableSemaphore()};
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = 1;
		VkCommandBuffer CB = commandBuffer.GetVKCommandBuffer();
		submitInfo.pCommandBuffers = &CB;

		VkSemaphore signalSemaphores[] = { swapchain.GetRenderingCompleteSemaphore() };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		VkQueue graphicsQueue = logicalDevice.GetGraphicsQueue();

		// As soon as you call the following function, Vulkan starts rendering onto swapchain images.
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFence) != VK_SUCCESS)
		{
			std::cerr << "Failed to submit draw command buffer!" << std::endl;
			__debugbreak();
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = { swapchain.GetVKSwapchain() };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;

		presentInfo.pResults = nullptr; // Optional

		VkResult result = vkQueuePresentKHR(graphicsQueue, &presentInfo); // graphics and present queues are the same in my computer.

		if (result != VK_SUCCESS)
		{
			std::cerr << "Failed to present swap chain image!" << std::endl;
			__debugbreak();
		}
	}

	vkDeviceWaitIdle(logicalDevice.GetVKDevice());
	return 0;
}