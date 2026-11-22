#pragma once
#include "RendererType.h" // lives next to Engine.h in the public include dir
#include "Renderer/Renderer.h"
#include "VulkanContext.h"
#include "core.h"

struct FrameSync
{
    uint32_t                 ConcurrentAllowedFrameCount = MAX_FRAMES_IN_FLIGHT;
    uint32_t                 CurrentBufferIndex          = 0;
    std::vector<VkSemaphore> RenderingCompleteSemaphores;
    std::vector<VkSemaphore> AcquireFinishedSemaphores;
    std::vector<VkFence>     InFlightFences;
};

class EngineInternal
{
   public:
    static VulkanContext& GetContext();

    ~EngineInternal();

   private:
    EngineInternal()                                 = default;
    EngineInternal(const EngineInternal&)            = delete;
    EngineInternal(EngineInternal&&)                 = delete;
    EngineInternal& operator=(const EngineInternal&) = delete;
    EngineInternal& operator=(EngineInternal&&)      = delete;

    void Init(RendererType InRendererType);
    void Run();

    float DeltaTime();
    void  PollEvents();
    void  CreateSynchronizationPrimitives();

    // Typed access for the parts of the Engine API that only exist on the rasterizer (materials, particles,
    // raster model bindings). Null when another renderer is active. The only place we downcast _Renderer.
    VulkanForwardRenderer* TryGetRasterizer() const;

    // The single source of truth for which renderer is active: ask it via _Renderer->GetType().
    Unique<RendererInterface> _Renderer = nullptr;

    Unique<VulkanContext> _Context       = nullptr;
    Ref<class Swapchain>  _Swapchain     = nullptr;
    Ref<class Camera>     _Camera        = nullptr;
    Ref<class Scene>      _ActiveScene   = nullptr;
    float                 _LastFrameTime = 0.0f;

    friend class Engine;
};
