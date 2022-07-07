#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <optional>
#include <set>

#include <cstdint>
#include <limits>
#include <algorithm>

#include <fstream>
#include <filesystem>

// Window width, height
const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;

// Limit of concurrent frames that will be processed.
const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers =
{
    "VK_LAYER_KHRONOS_validation"
};
const std::vector<const char*> deviceExtensions =
{
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
const std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_LINE_WIDTH
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static std::vector<char> ReadFile(const std::string& filePath)
{
    // ate : Start reading at the end of the file
    // binary: read the file as binary, don't do char conversion.
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    
    //std::cout << std::filesystem::current_path() << std::endl;

    if (!file.is_open())
    {
        std::cerr << "Error:" << errno << std::endl;
        throw std::runtime_error("Failed to open file!");
    }
    // tellg(): tellg() gives you the current position of the reader head. In this case we opened the file by starting to read if from the end so it gives us the size.
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize); // Reads the contents of "file" into buffer with the help of buffer.data() pointer. The size of the reading is "fileSize";

    file.close();

    return buffer;
}
/// <summary>
/// A Vulkan callback function. This will be called by the validation layer whenever an error occurs to print the error message.
/// </summary>
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

    return VK_FALSE;
}
/// <summary>
/// A custom proxy function that returns the address of native Vulkan function: "vkCreateDebugUtilsMessengerEXT" and calls that function to setup the debug messenger with the passed params.
/// </summary>
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}
/// <summary>
/// Another custom proxy function to destroy a "VkDebugUtilsMessengerEXT" object. This proxy function loads the "vkDestroyDebugUtilsMessengerEXT" function from Vulkan and uses it.
/// </summary>
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}
class HelloTriangleApplication {
    /// <summary>
    /// A struct that contains indices for the queue families our Vulkan program needs.
    /// </summary>
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily; // A queue family that contains rendering commands.
        std::optional<uint32_t> presentFamily; // A family that is used to present rendered images to a VkSurface.

        bool IsComplete()
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };
    /// <summary>
    /// A custom struct that contains information about swap chain details. Members are: capabilities, formats and present modes.
    /// </summary>
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };
public:
    void Run()
    {
        // Step 1
        InitWindow();
        // Step 2
        InitVulkan();
        // Step 3
        MainLoop();
        // Step 4
        Cleanup();
    }
private:
    GLFWwindow*                  m_Window = nullptr; // Handle to our GLFW window.
    VkInstance                   m_Instance; // Instances are needed to link the hardware of the computer to Vulkan API. This is the thing that does the communication between the two.
    VkDebugUtilsMessengerEXT     m_DebugMessenger; // A messenger object handle that you can have as many as you want. This one is for debug messages.
    VkPhysicalDevice             m_PhysicalDevice = VK_NULL_HANDLE; // A physical device that we are going to run Vulkan commands on. This is basically the GPU.
    VkDevice                     m_Device; // A handle to our logical device.
    VkQueue                      m_GraphicsQueue; // A handle to our graphics family queue.
    VkQueue                      m_PresentQueue; // A handle to our present family queue.
    VkSurfaceKHR                 m_Surface; // The surface where we will draw our images to.
    VkSwapchainKHR               m_SwapChain; 
    std::vector<VkImage>         m_SwapChainImages; 
    std::vector<VkImageView>     m_SwapChainImageViews;
    VkFormat                     m_SwapChainImageFormat;
    VkExtent2D                   m_SwapChainExtent;
    VkRenderPass                 m_RenderPass;
    VkPipelineLayout             m_PipelineLayout;
    VkPipeline                   m_GraphicsPipeline;
    std::vector<VkFramebuffer>   m_SwapChainFramebuffers;
    VkCommandPool                m_CommandPool;
    std::vector<VkCommandBuffer> m_CommandBuffers;
    std::vector<VkSemaphore>     m_ImageAvailableSemaphores;
    std::vector<VkSemaphore>     m_RenderFinishedSemaphores;
    std::vector<VkFence>         m_InFlightFences; // A fence that tells whether a frame is currently being drawn.
    uint32_t                     m_CurrentFrame = 0;
    bool                         m_FramebufferResized = false;
private:
    /// <summary>
    /// Initializes a GLFW window.
    /// </summary>
    void InitWindow()
    {
        if (!glfwInit())
        {
            std::cerr << "Could not initalize GLFW!\n";
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Tell it to not use OpenGL as the default API.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);   // Disable the resizing functionality.

        m_Window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Demo", nullptr, nullptr); // Return a pointer to the handle.

        glfwSetErrorCallback(glfw_error_callback);
        glfwSetWindowUserPointer(m_Window, this);
        glfwSetFramebufferSizeCallback(m_Window, glfw_framebuffer_resize_callback);


        if (!m_Window)
        {
            throw std::runtime_error("failed to create GLFW window");
        }
    }
    static void glfw_framebuffer_resize_callback(GLFWwindow* window, int width, int height)
    {
        HelloTriangleApplication* app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->m_FramebufferResized = true;
    }
    /// <summary>
    /// A GLFW error callback. Called whenever an error occurs related with GLFW windows.
    /// </summary>
    static void glfw_error_callback(int error, const char* description)
    {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    }
    void InitVulkan()
    {
        // Step 2.1
        CreateInstance();
        // Step 2.2
        SetupDebugMessenger();
        // Step 2.3
        CreateSurface();
        // Step 2.4
        PickPhysicalDevice();
        // Step 2.5
        CreateLogicalDevice();
        // Step 2.6
        CreateSwapChain();
        // Step 2.7
        CreateImageViews();
        // Step 2.8
        CreateRenderPass();
        // Step 2.9
        CreateGraphicsPipeline();
        // Step 2.10
        CreateFramebuffers();
        // Step 2.11
        CreateCommandPool();
        // Step 2.12
        CreateCommandBuffers();
        // Step 2.13
        CreateSyncObjects();
    }
    void MainLoop()
    {
        while (!glfwWindowShouldClose(m_Window))
        {
            glfwPollEvents(); // Checks for events like button presses.
            DrawFrame();
        }
        vkDeviceWaitIdle(m_Device);
    }
    void CleanupSwapChain()
    {
        for (auto framebuffer : m_SwapChainFramebuffers) {
            vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
        }
        vkDestroyPipeline(m_Device, m_GraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
        vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
        for (auto imageView : m_SwapChainImageViews)
        {
            vkDestroyImageView(m_Device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(m_Device, m_SwapChain, nullptr);
    }
    void Cleanup()
    {
        CleanupSwapChain();

        if (enableValidationLayers)
            DestroyDebugUtilsMessengerEXT(m_Instance, m_DebugMessenger, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(m_Device, m_ImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(m_Device, m_RenderFinishedSemaphores[i], nullptr);
            vkDestroyFence(m_Device, m_InFlightFences[i], nullptr);
        }

        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        vkDestroyDevice(m_Device, nullptr);
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr); // Make sure it is destroyed before the instance.
        vkDestroyInstance(m_Instance, nullptr);
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
    void RecreateSwapChain()
    {
        // Checks the case of window minimization. Wait until the window is in the foreground again before continuing.
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_Window, &width, &height);
        while (width == 0|| height == 0) {
            glfwGetFramebufferSize(m_Window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(m_Device);

        CreateSwapChain();
        CreateImageViews();
        CreateRenderPass();
        CreateGraphicsPipeline();
        CreateFramebuffers();
    }
    void CreateSyncObjects()
    {
        m_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(m_Device, &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }
    void RecordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_RenderPass;
        renderPassInfo.framebuffer = m_SwapChainFramebuffers[imageIndex];

        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = m_SwapChainExtent;

        VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // Record the following commands into the first parameter of each of those functions "commandBuffer"
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_GraphicsPipeline);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
    void DrawFrame()
    {
        vkWaitForFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX); // Wait for the "m_InFlightFence" to be signalled by GPU to continue. This fence is used to make sure we don't start drawing a 
        // new frame while the previous one is still being drawn.

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(m_Device, m_SwapChain, UINT64_MAX, m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            RecreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(m_Device, 1, &m_InFlightFences[m_CurrentFrame]);
        vkResetCommandBuffer(m_CommandBuffers[m_CurrentFrame], 0);
        RecordCommandBuffer(m_CommandBuffers[m_CurrentFrame], imageIndex); // Recording a command buffer every frame is said to be very efficient. So you can keep doing this every frame even if the commands inside it doesn't change.

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[m_CurrentFrame];

        VkSemaphore signalSemaphores[] = { m_RenderFinishedSemaphores[m_CurrentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;


        // As soon as you call the following function, Vulkan starts rendering onto swapchain images.
        if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { m_SwapChain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        presentInfo.pResults = nullptr; // Optional

        result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized)
        {
            m_FramebufferResized = false;
            RecreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
    void CreateCommandBuffers()
    {
        // We need command buffers for each frame that we are working on simultaneously.
        m_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t) m_CommandBuffers.size();

        if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }
    void CreateCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }
    void CreateFramebuffers()
    {
        m_SwapChainFramebuffers.resize(m_SwapChainImageViews.size());

        for (size_t i = 0; i < m_SwapChainImageViews.size(); i++)
        {

            VkImageView attachments[] = {
                m_SwapChainImageViews[i]
            };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_SwapChainExtent.width;
            framebufferInfo.height = m_SwapChainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapChainFramebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }
    void CreateRenderPass()
    {
        // Color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_SwapChainImageFormat; // Format of the color attachment should match the swap chain image format.
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // Colot attachment ref.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // Vulkan spilts a render pass into one or more subpasses. Each render pass has to has at least one subpass.
        // In our case, our color attachment pass is our only rendering pass. (This will get extended with a depth & stencil attachment when we start doing shadows.)
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1; // Number of attachments.
        renderPassInfo.pAttachments = &colorAttachment; // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }
    void CreateGraphicsPipeline()
    {
        // Read shaders from file.
        auto vertShaderCode = ReadFile("shaders/vert.spv");
        auto fragShaderCode = ReadFile("shaders/frag.spv");


        VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
        VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

        // Vertex shader stage info.
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;

        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        // Fragment shader stage info.
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // This "VkPipelineVertexInputStateCreateInfo" strcut specifies the format of the data that you are going to pass to the vertex shader.
        // Bindings: Spacing between data and whether the data is per-vertex or per-instance.
        // Attribute: Type of the attributes passed to the vertex shader, which binding to load them from and at which offset.
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional 

        // This struct assembels the points and forms whatever primitive you want to form. These primitives are usually: triangles, lines and points.
        // We are interested in drawing trianlgles in our case.
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // Viewport specifies the dimensions of the framebuffer that we want to draw to. This usually spans the entirety of a framebuffer.
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)m_SwapChainExtent.width;
        viewport.height = (float)m_SwapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Any pixel outside of the scissor boundaries gets discarded. Scissors act rather as filters and required to be specified.
        // If you do not want any kind of discarding due to scissors, make its extent the same as the framebuffer / swap chain images.
        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = m_SwapChainExtent;

        // The above two states "VkRect2D" and "VkViewport" need to be combined into a single struct called "VkPipelineViewportStateCreateInfo". That is 
        // exactly what we are doing here.
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;


        // This structure configures the rasterizer.
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // Multisampling configuration here.
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

        VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{}; // Empty for now.

        // Color blebding configuration. This blends the current color that the fragment shader returns with that is already present in the framebuffer with the 
        // parameters specified below. The below setting is usually what you need most of the time.
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // Dynamic states that can be changed during drawing time. The states that can be found in "dynamicStates" can be changed without recreateing the pipeline.
        // Viewport size (screen resizing) is a good example for this.
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        // This struct specifies how the "UNIFORM" variables are going to be laid out. TO DO: Learn it better.
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0; // Optional
        pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

        if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // This struct binds together all the other structs we have created so far in this function up above.
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr; // Optional

        pipelineInfo.layout = m_PipelineLayout;

        pipelineInfo.renderPass = m_RenderPass;
        pipelineInfo.subpass = 0;

        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        if (vkCreateGraphicsPipelines(m_Device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_GraphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(m_Device, fragShaderModule, nullptr);
        vkDestroyShaderModule(m_Device, vertShaderModule, nullptr);
    }
    /// <summary>
    /// Creates a shader module. In our scope it is either a fragment shader or a vertex shader. Shaders need to be wrapped with a "Shader Module" object in Vulkan.
    /// The binary code (SPIR-V) for the shaders are passed to their corresponding "Shader Modules".
    /// </summary>
    /// <param name="shaderCode"> Shader binary code after compiling the GLSL into SPIR-V.</param>
    /// <returns> A VkShaderModule </returns>
    VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode)
    {
        VkShaderModuleCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderCode.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(m_Device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create shader module");
        }
        return shaderModule;
    }
    /// <summary>
    /// To access the swap chain images, you need "Image Views" for each of those images. This helper function takes care of that part by creating 
    /// an image view for each swap chain image we have in the member variable m_SwapChainImages.
    /// </summary>
    void CreateImageViews()
    {
        m_SwapChainImageViews.resize(m_SwapChainImages.size());
        for (int i = 0; i < m_SwapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapChainImages[i];

            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_SwapChainImageFormat;

            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create image views.");
            }
        }
    }
    /// <summary>
    /// Fetches all the necessary information and stores them in a "SwapChainSupportDetails" struct. The fetched information are: "Physical Device Surface Capabilities", "Physical Device Surface Formats", "Physical Device 
    /// Present Modes".
    /// </summary>
    /// <param name="device">A physical device object.</param>
    /// <returns>A "SwapChainSupportDetails" containing the field members mentioned in the summary.</returns>
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_Surface, &details.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_Surface, &formatCount, details.formats.data());
        }


        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_Surface, &presentModeCount, details.presentModes.data());
        }
        return details;
    }
    /// <summary>
    /// Creates a surface with the help of a GLFW function. That GLFW function contains different implementations for each platform.
    /// </summary>
    void CreateSurface()
    {
        if (glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create a window surface");
        }
    }
    /// <summary>
    /// A helper function that chooses the best option for Surface Format for the swap chain.
    /// </summary>
    /// <param name="availableFormats"> A vector containing the possible options for the surface formats.</param>
    /// <returns>The best / preferred option depending on the implementation of this function.</returns>
    VkSurfaceFormatKHR ChooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        // First, try to find the preferred format here.
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        // If the above check fails choose the next best settings.
        return availableFormats[0];
    }
    /// <summary>
    /// A helper function that picks the preferred option for the swap chain present mode. There are four possible Present Modes in Vulkan:
    /// 1) "VK_PRESENT_MODE_IMMEDIATE_KHR"
    /// 2) "VK_PRESENT_MODE_FIFO_KHR"
    /// 3) "VK_PRESENT_FIFO_RELAXED_KHR"
    /// 4) "VK_PRESENT_MODE_MAILBOX_KHR"
    /// </summary>
    /// <param name="availablePresentModes">A vector containing all the possible present modes.</param>
    /// <returns>The best / preferred present mode depending on the implementation of this function.</returns>
    VkPresentModeKHR ChooseSwapChainPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;  // This mode is guaranteed to be present in all hardware so if the above preference could not be found, we roll back to this.
    }
    /// <summary>
    /// A helper function that picks the best / preferred option for the swap chain extent.
    /// </summary>
    /// <param name="capabilities">A vector containing all the possible capabilities.</param>
    /// <returns>The best option.</returns>
    VkExtent2D ChooseSwapChainExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) // TO DO: This part could be problematic. Two macros clash for some reason.
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_Window, &width, &height);

            VkExtent2D actualExtent =
            {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }
    /// <summary>
    /// Takes care of the part for creating a swap chain. First checks the swap chain support and then creates a "VkSwapchainCreateInfoKHR" struct and fills it up.
    /// </summary>
    void CreateSwapChain()
    {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

        VkSurfaceFormatKHR surfaceFormat = ChooseSwapChainSurfaceFormat(swapChainSupport.formats); // Surface Format = Color depth.
        VkPresentModeKHR presentMode = ChooseSwapChainPresentMode(swapChainSupport.presentModes); // Present Mode = Conditions for swapping images to the screen.
        VkExtent2D extent = ChooseSwapChainExtent(swapChainSupport.capabilities); // Extent = Resolution of images in swap chain.

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1; // Here we are requesting the minimum number of images for the driver to function + 1.This is to ensure that while the Vulkan driver completes 
        //the internal operations we have at least one more free image slot to render to.

        // This if check is here to make sure we don't exceed the maximum supported image count. "maxImageCount = 0" means there is no maximum.
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        // Connecting the swap chain with the surface.
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;

        // Details of swap chain images
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1; // Specifies the amount of layers each image consists of.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // Specifies what kind of operations we'll use the images in the swap chain for.

        QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            // Concurrent mode requires you to specifc "queueFamilyIndexCount" and "pQueueFamilyIndices".
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0; // Optional.
            createInfo.pQueueFamilyIndices = nullptr; // Optional.
        }
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // Specifies whether you want to do a pre-transformation operation on images like 90 degree clockwise rotation or horizontal flip.
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // Specifies whether the alpha channel should be used to blend with "OTHER WINDOWS" on your pc.
        createInfo.presentMode = presentMode; 
        createInfo.clipped = VK_TRUE; // Specifies whether to clip the pixels in front of the Vulkan application. This has nothing to do with the rendered images.
        createInfo.oldSwapchain = VK_NULL_HANDLE; // Look it up in https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain

        if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, nullptr);
        m_SwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device, m_SwapChain, &imageCount, m_SwapChainImages.data());

        m_SwapChainImageFormat = surfaceFormat.format;
        m_SwapChainExtent = extent;
    }
    /// <summary>
    /// Checks if all the extensions located in the vector "deviceExtensions" are present in the available extension list.
    /// </summary>
    /// <param name="device">A physical device object.</param>
    /// <returns>A bool indicating whether the extensions could be found.</returns>
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);

        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto& extension : availableExtensions)
        {
            if (requiredExtensions.find(extension.extensionName) != requiredExtensions.end())
            {
                std::cout << "Extension: " << extension.extensionName << " is supported." << std::endl;
            }
            auto pointedExtension = requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty(); // If the vector is empty that means all the required extensions have been found.
    }
    /// <summary>
    /// A helper function that finds and returns a custom struct "QueueFamilyIndices" that contains all the necessatry queue families that the Vulkan program needs.
    /// </summary>
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices queueFamilyIndices;
        uint32_t queueFamilyCount = 0;

        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());


        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) // Checks the flags of the queue and ANDs the desired functionality with that flag.
            {
                queueFamilyIndices.graphicsFamily = i;
            }
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);

            if (presentSupport)
            {
                queueFamilyIndices.presentFamily = i;
            }
            if (queueFamilyIndices.IsComplete())
            {
                break;
            }
            i++;
        }

        return queueFamilyIndices;
    }
    /// <summary>
    /// Gets all the physical devices on the computer and finds the most suitable one.
    /// </summary>
    void PickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for (const auto& device : devices)
        {
            if (IsDeviceSuitable(device))
            {
                m_PhysicalDevice = device;
                break;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        else
        {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
            std::cout << "Found a suitable GPU: " << deviceProperties.deviceName << std::endl;
        }
    }
    /// <summary>
    /// Checks whether a device is suitable to be used. It checks whether desired queue family and swap chain functionalities are present in the GPU.
    /// </summary>
    bool IsDeviceSuitable(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices = FindQueueFamilies(device);

        bool swapChainAdequate = false;
        bool extensionsSupported = CheckDeviceExtensionSupport(device);
        if (extensionsSupported)
        {
            // Query the suitabilty of the swap chain only after you determine the extensions are supported.
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        return indices.IsComplete() && extensionsSupported && swapChainAdequate;
    }
    /// <summary>
    /// A helper function for the step at where we create a logical device for our Vulkan program. This function will first create a device command queue and 
    /// populate its createInfo with the settings we want (in this case we are only creating a Graphics Family Queue). After that is completed a device features struct is created.
    /// And finally we create the actual Create Info for the logical device where we pass the previous structs we have created. And depending on whether the validation layers
    /// are enabled we can also optionally enable debug functionality here as well.
    /// </summary>
    void CreateLogicalDevice()
    {
        QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos; // Hold all the queue create infos in this vector.
        std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() }; // A set that holds unique queue family indices. If two families are supporoted by the same index, then the set will store them as one single family.

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) // Loop over the families we found above and create a "VkDeviceQueueCreateInfo" for each.
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo); // Add the freshly created "VkDeviceQueueCreateInfo" to the vector.
        }

        VkPhysicalDeviceFeatures deviceFeatures{}; // Empty for now.

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()); // The number of queue families stored inside the vector.
        createInfo.pQueueCreateInfos = queueCreateInfos.data(); //A pointer to the beginning of the vector that we store our queue families in.

        createInfo.pEnabledFeatures = &deviceFeatures;

        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }
        if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        // In here you are literally just getting a single queue from a queue family. In the case of my computer (NVIDIA RTX 3060) I have 16 distinct queues in my queue family 0.
        // So I can literally fetch 16 different queues from that family with the following lines of codes. I only need to pass the index of the queue family and then which queue OUT OF THOSE 16 I want to fetch.
        // If you know the number of queues you have in a queue family you can hand pick one by providing its index as the third parameter down below. Easy stuff.

        vkGetDeviceQueue(m_Device, indices.graphicsFamily.value(), 0, &m_GraphicsQueue); // This is how you extract a specific queue from a logical device.
        vkGetDeviceQueue(m_Device, indices.presentFamily.value(), 0, &m_PresentQueue); // This is how you extract a specific queue from a logical device.
    }
    /// <summary>
    /// Creates the instance that connects Vulkan API to computer drivers. This step should be the first step you do when initializing Vulkan.
    /// </summary>
    void CreateInstance()
    {
        // 2.1.1 - Does a small check depending on whether we want to enable validation layers in our program. This depends on the mode of the program DEBUG or RELEASE.
        // It also checks whether the system this code was run on supports validation layer.
        if (enableValidationLayers && !CheckValidationLayerSupport())
        {
            throw std::runtime_error("Validation layers requsted, but not available!");
        }
        // 2.1.1 - ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // 2.1.2 - Creating the Application Info structure here that later on would be passed to the Instance Create Info structure. 
        // This struct is optional  but it allows Vulkan to better optimize your program if you use one.
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;
        // 2.1.2 - /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        appInfo.pNext;

        // 2.1.3 - Taking care of the Create Info here. A strcut is created first and then the corresponding fields are filled with the wanted behavior along with the "appInfo"
        // we just created above. This struct tells Vulkan which global extensions and validation layers we want to use. This struct is not optional and needs to be created.
        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        std::vector<const char*> extensions = GetRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // 2.1.3.1 - Creating a "VkDebugUtilsMessengerCreateInfoEXT" object here with the aim of extending the createInfo we just created above.
        // We are going to pass this "debugCreateInfo" to the pNext pointer of the "createInfo" to extend it with the functionality of the validation layer.
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
        }
        else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }
        // 2.1.3.1 /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
        // 2.1.3 - ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // 2.1.4 - Print availabe extension in this system.
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
        std::cout << "Available extensions: \n";
        for (const auto& extension : availableExtensions)
        {
            std::cout << '\t' << extension.extensionName << '\n';
        }
        // 2.1.4 - ////////////////////////////////////////
    }
    /// <summary>
    /// This function fills up an empty "VkDebugUtilsMessengerCreateInfoEXT" object.
    /// </summary>
    /// <param name="createInfo"> Pass an empty "VkDebugUtilsMessengerCreateInfoEXT" pointer here for this function to fill it up.</param> 
    void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |*/ VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = DebugCallback;
    }
    /// <summary>
    /// First creates a create info for the "VkDebugUtilsMessengerEXT" object and then creates the actual messenger object with that info.
    /// </summary>
    void SetupDebugMessenger()
    {
        if (!enableValidationLayers) return;
        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        PopulateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(m_Instance, &createInfo, nullptr, &m_DebugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }
    /// <summary>
    /// A helper function that checks whether the system this code is being run on has support for the validation layers defined at the beginning of the 
    /// program inside the struct "validationLayers".
    /// </summary>
    bool CheckValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        std::cout << "Supported layers: \n";

        for (const char* layerName: validationLayers)
        {
            bool layerFound = false;

            for (const auto& layerProperties : availableLayers)
            {
                std::cout << layerProperties.layerName << std::endl;
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }
        return true;
    }
    /// <summary>
    /// The returned variable is passed to the "ppEnabledExtensions" field of a Create Info.
    /// In this case it is passed to a "VkInstanceCreateInfo" object. Check where this function is being used to better understand.
    /// </summary>
    /// <returns> A vector of const char* that contains the names of the required extensions. GLFW extensions are also included in here by default. </returns>
    std::vector<const char*> GetRequiredExtensions()
    {
        const char** glfwExtensions;
        uint32_t extensionCount = 0;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&extensionCount);
        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + extensionCount);
        std::cout << "Required GLFW extensions: \n";
        for (const auto& extension : extensions)
        {
            std::cout << extension << std::endl;
        }

        if (enableValidationLayers)
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }
};

//int main() {
//    HelloTriangleApplication app;
//
//    try {
//        app.Run();
//    }
//    catch (const std::exception& e) {
//        std::cerr << e.what() << std::endl;
//        return EXIT_FAILURE;
//    }
//
//    return EXIT_SUCCESS;
//}