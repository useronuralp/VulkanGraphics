#pragma once
#include "core.h"
#include "VulkanContext.h"

#include <vector>
#include <vulkan/vulkan.h>

class Swapchain
{
   public:
    Swapchain(const Swapchain&)            = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    explicit Swapchain(VulkanContext& InContext);
    ~Swapchain() noexcept;

    void Recreate();
    void Cleanup();

   public:
    const VkSwapchainKHR            GetHandle() const noexcept;
    const VkFormat                  GetImageFormat() const noexcept;
    const VkPresentModeKHR          GetPresentMode() const noexcept;
    const std::vector<VkImage>&     GetImages() const noexcept;
    const std::vector<VkImageView>& GetImageViews() const noexcept;

   private:
    void Create();

   private:
    VulkanContext&           _Context;
    VkSwapchainKHR           _Swapchain = VK_NULL_HANDLE;
    std::vector<VkImage>     _Images;
    std::vector<VkImageView> _ImageViews;
    uint32_t                 _ImageCount;
    VkFormat                 _Format;
    VkPresentModeKHR         _PresentMode;
};
