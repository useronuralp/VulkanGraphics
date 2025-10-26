#pragma once
#include "core.h"
#include "Scene.h"

class Engine
{
   public:
    void           Init();
    void           Run();
    static Engine& Get();

    Ref<class Scene> CreateScene() {};
    Ref<class Scene> GetActiveScene() const {};
    void             SetActiveScene(Ref<class Scene> InScene) {};

    Engine();
    ~Engine() = default;

   private:
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    friend class EngineInternal;
    Unique<class EngineInternal> _Internal;
};