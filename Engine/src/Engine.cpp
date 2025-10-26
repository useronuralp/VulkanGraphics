#include "Engine.h"
#include "EngineInternal.h"

// Create with new because EngineInternal constructor is private and can only be accessed from this class
Engine::Engine() : _Internal(Unique<EngineInternal>(new EngineInternal()))
{
}

Engine& Engine::Get()
{
    // This uses the constructor above which initializes _Internal.
    static Engine instance;
    return instance;
}

void Engine::Init()
{
    _Internal->Init();
}

void Engine::Run()
{
    _Internal->Run();
}