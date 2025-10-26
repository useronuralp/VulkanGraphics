#include "Engine.h"
#include "EngineInternal.h"

Engine& Engine::Get()
{
    static Engine instance;
    return instance;
}

// Create with new because EngineInternal constructor is private and can only be accessed from this class
Engine::Engine() : _Internal(Unique<EngineInternal>(new EngineInternal()))
{
}

void Engine::Init()
{
    _Internal->Init();
}

void Engine::Run()
{
    _Internal->Run();
}