#pragma once
#include "core.h"

struct FrameSync
{
    uint32_t                 ConcurrentAllowedFrameCount = MAX_FRAMES_IN_FLIGHT;
    uint32_t                 CurrentBufferIndex          = 0;
    std::vector<VkSemaphore> RenderingCompleteSemaphores;
    std::vector<VkSemaphore> AcquireFinishedSemaphores;
    std::vector<VkFence>     InFlightFences;
};

class VulkanContext;
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

    void Init();
    void Run();

    float CalculateDeltaTime();
    void  PollEvents();
    void  CreateSynchronizationPrimitives();

    Ref<class RendererInterface> _Renderer      = nullptr;
    Ref<class Swapchain>         _Swapchain     = nullptr;
    Ref<class Camera>            _Camera        = nullptr;
    Ref<VulkanContext>           _Context       = nullptr;
    Ref<class Scene>             _ActiveScene   = nullptr;
    float                        _LastFrameTime = 0.0f;

    friend class Engine;
};
