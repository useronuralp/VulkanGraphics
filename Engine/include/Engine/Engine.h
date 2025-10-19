#pragma once
#include "core.h"
#include "Scene.h"

#include <memory>

class VulkanContext;
class Swapchain;
class Camera;
class Renderer;
class Scene;

class Engine
{
   public:
    void Init();
    void Run();

    static Engine& Get();

    // Scene API
    Ref<Scene> CreateScene() {};
    Ref<Scene> GetActiveScene() const {};
    void       SetActiveScene(Ref<Scene> InScene) {};

    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

   private:
    Unique<Renderer>      _Renderer  = nullptr;
    Ref<Swapchain>        _Swapchain = nullptr;
    Ref<Camera>           _Camera    = nullptr;
    Unique<VulkanContext> _Context   = nullptr;

    Ref<Scene> _ActiveScene          = nullptr;

   private:
    Engine() = default;
    void Shutdown();

    float CalculateDeltaTime();

   private:
    float _LastFrameTime = 0.0f;

    friend class EngineInternal;
};