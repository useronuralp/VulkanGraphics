#pragma once
#include "core.h"
#include "Scene.h"

#include <memory>

class VulkanContext;
class Swapchain;
class Camera;
class RendererInterface;
class Scene;

class Engine
{
   public:
    void           Init();
    void           Run();
    static Engine& Get();

    Ref<Scene> CreateScene() {};
    Ref<Scene> GetActiveScene() const {};
    void       SetActiveScene(Ref<Scene> InScene) {};

   private:
    Engine()                         = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

   private:
    void  Shutdown();
    float CalculateDeltaTime();
    void  PollEvents();
    void  CreateSynchronizationPrimitives();

   private:
    Unique<RendererInterface> _Renderer      = nullptr;
    Ref<Swapchain>            _Swapchain     = nullptr;
    Ref<Camera>               _Camera        = nullptr;
    Unique<VulkanContext>     _Context       = nullptr;
    Ref<Scene>                _ActiveScene   = nullptr;
    float                     _LastFrameTime = 0.0f;

    friend class EngineInternal;
};