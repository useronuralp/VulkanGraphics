#pragma once
#include "core.h"

class Engine
{
   public:
    // public API
    void           Init();
    void           Run();
    static Engine& Get();

    Ref<class Scene> CreateScene() {};
    Ref<class Scene> GetActiveScene() const {};
    void             SetActiveScene(Ref<class Scene> InScene) {};
    // ~public API end

   private:
    Engine();
    ~Engine()                        = default;
    Engine(const Engine&)            = delete;
    Engine& operator=(const Engine&) = delete;

    friend class EngineInternal;
    Unique<class EngineInternal> _Internal;
};