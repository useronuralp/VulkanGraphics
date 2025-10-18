#pragma once
#include <memory>

class VulkanContext;
class Swapchain;
class Camera;
class Renderer;

class Engine
{
   public:
    void Init();
    void Run();

   public:
    static VulkanContext& GetContext();

   private:
    void Shutdown();

   private:
    static std::unique_ptr<VulkanContext> _Context;

   public:
    static std::unique_ptr<Renderer> _Renderer;
    std::shared_ptr<Swapchain>       _Swapchain = nullptr;
    std::shared_ptr<Camera>          _Camera    = nullptr;

   public:
    // FIX: Swapchain depends on this;.
    static uint32_t GetActiveImageIndex();
};