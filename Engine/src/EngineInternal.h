#pragma once
#include "Engine.h"
#include "VulkanContext.h"

#define MAX_FRAMES_IN_FLIGHT 3

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
    static VulkanContext& GetContext()
    {
        return *Engine::Get()._Context;
    }
};
