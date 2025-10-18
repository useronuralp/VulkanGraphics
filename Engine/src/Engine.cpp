#include "Camera.h"
#include "Engine.h"
#include "Renderer/Renderer.h"
#include "Surface.h"
#include "VulkanContext.h"

void Engine::Init()
{
    _Context = std::make_unique<VulkanContext>();
    _Context->Init();

    _Swapchain = make_s<Swapchain>(*_Context);

    _Camera =
        make_s<Camera>(45.0f, _Context->GetSurface()->GetVKExtent().width / (float)_Context->GetSurface()->GetVKExtent().height);

    _Renderer = std::make_unique<Renderer>(*_Context, _Swapchain, _Camera);
    _Renderer->Init();
    // TODO: Move out of renderer into UI layer.
    _Renderer->InitImGui();
}

void Engine::Run()
{
    while (!_Context->GetWindow()->ShouldClose())
    {
        float deltaTime = CalculateDeltaTime();

        _Renderer->PollEvents();
        if (!_Renderer->BeginFrame())
        {
            continue;
        }

        _Renderer->RenderImGui();

        _Renderer->RenderFrame(deltaTime);

        _Renderer->EndFrame();
    }

    Shutdown();
}

VulkanContext& Engine::GetContext()
{
    return *_Context;
}

Engine& Engine::GetEngine()
{
    static Engine instance;
    return instance;
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

float Engine::CalculateDeltaTime()
{
    const float currentTime = static_cast<float>(glfwGetTime());
    const float deltaTime   = currentTime - _LastFrameTime;
    _LastFrameTime          = currentTime;
    return deltaTime;
}
