#include "Camera.h"
#include "Device.h"
#include "DeviceRequirements.h"
#include "Engine.h"
#include "EngineInternal.h"
#include "Renderer/PathTracer.h"
#include "Renderer/Renderer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "VulkanContext.h"
#include "Window.h"

#include <vulkan/vulkan.h>

FrameSync GFrameSync;

VulkanContext& EngineInternal::GetContext()
{
    ENSURE(Engine::Get()._Internal->_Context, "Invalid context");
    return *Engine::Get()._Internal->_Context;
}

VulkanForwardRenderer* EngineInternal::TryGetRasterizer() const
{
    if (_Renderer && _Renderer->GetType() == RendererType::Rasterizer)
    {
        return static_cast<VulkanForwardRenderer*>(_Renderer.get());
    }
    return nullptr;
}

namespace
{
DeviceRequirements ResolveDeviceRequirementsFor(RendererType InRendererType)
{
    DeviceRequirements requirements;
    switch (InRendererType)
    {
        case RendererType::Rasterizer:
            
            requirements.Extensions                               = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
            requirements.Features.Core.features.samplerAnisotropy = VK_TRUE;
            requirements.Features.Core.features.geometryShader    = VK_TRUE; // point-light shadow cubemap pass
            requirements.Features.Vulkan13.dynamicRendering       = VK_TRUE;
            return requirements;

        case RendererType::PathTracer:

            requirements.RayTracing = true;
            requirements.Extensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, // BLAS/TLAS objects and their build commands
                VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, // raygen/miss/hit shader stages, SBT, vkCmdTraceRaysKHR
                VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, // hard dependency of VK_KHR_acceleration_structure
            };

            auto& features                                       = requirements.Features;
            features.Core.features.samplerAnisotropy             = VK_TRUE; // material texture sampling (later parts)
            features.Vulkan12.bufferDeviceAddress                = VK_TRUE; // AS inputs, scratch, SBT are all passed as raw GPU addresses
            features.AccelerationStructure.accelerationStructure = VK_TRUE;
            features.RayTracingPipeline.rayTracingPipeline       = VK_TRUE;
            return requirements;
    }
    ENSURE(false, "Unhandled RendererType.");
    return {};
}

Unique<RendererInterface> CreateRenderer(RendererType InRendererType, VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
{
    switch (InRendererType)
    {
        case RendererType::Rasterizer:
            return make_u<VulkanForwardRenderer>(InContext, InSwapchain, InCamera);
        case RendererType::PathTracer:
            return make_u<VulkanPathTracer>(InContext, InSwapchain, InCamera);
    }
    ENSURE(false, "Unhandled RendererType.");
    return nullptr;
}
} // namespace

void EngineInternal::Init(RendererType InRendererType)
{
    // The renderer decides what the device must support, so ask it *before* the device exists.
    _Context = make_u<VulkanContext>();
    _Context->Init(ResolveDeviceRequirementsFor(InRendererType));

    _Swapchain = make_s<Swapchain>(*_Context);
    _Camera    = make_s<Camera>(45.0f, _Context->GetSurface()->GetVKExtent().width / (float)_Context->GetSurface()->GetVKExtent().height);


    _Renderer = CreateRenderer(InRendererType, *_Context, _Swapchain, _Camera);

    if (_Renderer->GetType() == RendererType::PathTracer)
    {
        _Camera->SetCameraRotationSpeedScale(0.05f);
    }

    // Sanity check on renderer type.
    ENSURE(_Renderer->GetType() == InRendererType, "Renderer::GetType() disagrees with the type that was constructed.");

    _Renderer->Init();

    // TODO: Move out of renderer into UI layer.
    _Renderer->InitImGui();

    CreateSynchronizationPrimitives(); // TODO redo
}

void EngineInternal::Run()
{
    while (NOT _Context->GetWindow()->ShouldClose())
    {
        float deltaTime = DeltaTime();

        PollEvents();

        if (NOT _Renderer->BeginFrame())
        {
            CreateSynchronizationPrimitives(); // TODO redo
            continue;
        }

        _Renderer->RenderImGui();
        _Renderer->RenderFrame(deltaTime);
        _Renderer->EndFrame();

        GFrameSync.CurrentBufferIndex % GFrameSync.ConcurrentAllowedFrameCount;
    }
}
EngineInternal::~EngineInternal()
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

void EngineInternal::CreateSynchronizationPrimitives()
{
    auto device     = _Context->GetDevice()->GetVKDevice();
    auto imageCount = _Swapchain->GetImageCount();

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
        ENSURE(
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &GFrameSync.RenderingCompleteSemaphores[i]) == VK_SUCCESS,
            "Failed to create rendering complete semaphore.");
    }

    for (int i = 0; i < GFrameSync.ConcurrentAllowedFrameCount; i++)
    {
        ENSURE(vkCreateFence(device, &fenceCreateInfo, nullptr, &GFrameSync.InFlightFences[i]) == VK_SUCCESS, "Failed to create is rendering fence.");
        ENSURE(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &GFrameSync.AcquireFinishedSemaphores[i]) == VK_SUCCESS, "Failed to create image available semaphore.");
    }
}

void EngineInternal::PollEvents()
{
    glfwPollEvents();
}

float EngineInternal::DeltaTime()
{
    const float currentTime = static_cast<float>(glfwGetTime());
    const float deltaTime   = currentTime - _LastFrameTime;
    _LastFrameTime          = currentTime;
    return deltaTime;
}
