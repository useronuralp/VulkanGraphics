#include "Camera.h"
#include "Engine.h"
#include "Renderer.h"
#include "Surface.h"
#include "VulkanContext.h"

std::unique_ptr<VulkanContext> Engine::_Context  = nullptr;
std::unique_ptr<Renderer>      Engine::_Renderer = nullptr;

void Engine::Init()
{
    _Context = std::make_unique<VulkanContext>();
    _Context->Init();

    _Swapchain = std::make_shared<Swapchain>(*_Context);

    _Camera    = std::make_shared<Camera>(
        45.0f, _Context->GetSurface()->GetVKExtent().width / (float)_Context->GetSurface()->GetVKExtent().height);

    _Renderer = std::make_unique<Renderer>(*_Context, _Swapchain, _Camera);
    _Renderer->Init();
    // TO DO: Move out of renderer into UI layer.
    _Renderer->InitImGui();
}

void Engine::Shutdown()
{
}

void Engine::Run()
{
    while (!_Context->GetWindow()->ShouldClose()) {
        _Renderer->PollEvents();
        if (!_Renderer->BeginFrame()) {
            continue;
        }

        _Renderer->RenderImGui();

        _Renderer->EndFrame();
    }

    _Renderer->Cleanup();
    Shutdown();
}

uint32_t Engine::GetActiveImageIndex()
{
    return _Renderer->m_ActiveImageIndex;
}

VulkanContext& Engine::GetContext()
{
    return *_Context;
}