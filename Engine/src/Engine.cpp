#include "Camera.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Renderer/Renderer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "VulkanContext.h"
#include "Window.h"

FrameSync GFrameSync;

Engine& Engine::Get()
{
    static Engine instance;
    return instance;
}

void Engine::Init()
{
    _Context = std::make_unique<VulkanContext>();
    _Context->Init();

    _Swapchain = make_s<Swapchain>(*_Context);

    _Camera =
        make_s<Camera>(45.0f, _Context->GetSurface()->GetVKExtent().width / (float)_Context->GetSurface()->GetVKExtent().height);

    _Renderer = std::make_unique<ForwardRenderer>(*_Context, _Swapchain, _Camera);
    _Renderer->Init();
    // TODO: Move out of renderer into UI layer.
    _Renderer->InitImGui();

    CreateSynchronizationPrimitives(); // TODO redo
}

void Engine::Run()
{
    while (!_Context->GetWindow()->ShouldClose())
    {
        float deltaTime = CalculateDeltaTime();

        PollEvents();

        if (!_Renderer->BeginFrame())
        {
            CreateSynchronizationPrimitives(); // TODO redo
            continue;
        }

        _Renderer->RenderImGui();

        _Renderer->RenderFrame(deltaTime);

        _Renderer->EndFrame();

        GFrameSync.CurrentBufferIndex % GFrameSync.ConcurrentAllowedFrameCount;
    }

    Shutdown();
}

float Engine::CalculateDeltaTime()
{
    const float currentTime = static_cast<float>(glfwGetTime());
    const float deltaTime   = currentTime - _LastFrameTime;
    _LastFrameTime          = currentTime;
    return deltaTime;
}

void Engine::CreateSynchronizationPrimitives()
{
    auto device     = Engine::Get()._Context->GetDevice()->GetVKDevice();
    auto imageCount = Engine::Get()._Swapchain->GetImageCount();

    // Cleanup
    for (int i = 0; i < GFrameSync.AcquireFinishedSemaphores.size(); i++)
    {
        vkDestroySemaphore(device, GFrameSync.AcquireFinishedSemaphores[i], nullptr);
    }
    for (int i = 0; i < GFrameSync.InFlightFences.size(); i++)
    {
        vkDestroyFence(device, GFrameSync.InFlightFences[i], nullptr);
    }
    for (int i = 0; i < GFrameSync.RenderingCompleteSemaphores.size(); i++)
    {
        vkDestroySemaphore(device, GFrameSync.RenderingCompleteSemaphores[i], nullptr);
    }

    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    GFrameSync.RenderingCompleteSemaphores.resize(imageCount);
    GFrameSync.AcquireFinishedSemaphores.resize(GFrameSync.ConcurrentAllowedFrameCount);
    GFrameSync.InFlightFences.resize(GFrameSync.ConcurrentAllowedFrameCount);

    for (int i = 0; i < imageCount; i++)
    {
        ASSERT(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &GFrameSync.RenderingCompleteSemaphores[i]) == VK_SUCCESS,
            "Failed to create rendering complete semaphore.");
    }

    for (int i = 0; i < GFrameSync.ConcurrentAllowedFrameCount; i++)
    {
        ASSERT(
            vkCreateFence(device, &fenceCreateInfo, nullptr, &GFrameSync.InFlightFences[i]) == VK_SUCCESS,
            "Failed to create is rendering fence.");
        ASSERT(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &GFrameSync.AcquireFinishedSemaphores[i]) == VK_SUCCESS,
            "Failed to create image available semaphore.");
    }
}

void Engine::Shutdown()
{
    // Order matters here
    if (_Renderer)
    {
        _Renderer->Cleanup();
        _Renderer.reset();
    }

    if (_Swapchain)
    {
        _Swapchain->Cleanup();
        _Swapchain.reset();
    }

    if (_Camera)
    {
        _Camera.reset();
    }

    if (_Context)
    {
        _Context->Shutdown();
        _Context.reset();
    }
}

void Engine::PollEvents()
{
    glfwPollEvents();
}