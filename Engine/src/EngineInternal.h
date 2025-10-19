#pragma once
#include "Engine.h"
#include "VulkanContext.h"

class EngineInternal
{
   public:
    static VulkanContext& GetContext()
    {
        return *Engine::Get()._Context;
    }
};
