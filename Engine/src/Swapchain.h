#pragma once
#include "core.h"
#include "VulkanContext.h"

#include <vector>
#include <vulkan/vulkan.h>
class GLFWwindow;
class Framebuffer;
class Swapchain {
   public:
    Swapchain(const VulkanContext& InContext);
    ~Swapchain();

    void Recreate();
    void Cleanup();

   public:
    const VkSwapchainKHR                 GetVKSwapchain() const;
    const VkFormat                       GetSwapchainImageFormat() const;
    const VkPresentModeKHR               GetPresentMode() const;
    const std::vector<VkImage>&          GetSwapchainImages() const;
    const std::vector<VkImageView>&      GetSwapchainImageViews() const;
    VkRenderPass&                        GetSwapchainRenderPass();
    const std::vector<Ref<Framebuffer>>& GetFramebuffers() const;
    // void OnResize();
    const VkFramebuffer& GetActiveFramebuffer();

   private:
    void Init();
    // void CleanupSwapchain();
    VkFormat FindDepthFormat();
    VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
    bool     HasStencilComponent(VkFormat format);

   private:
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;

    // Variables needed for images / framebuffers.
    std::vector<Ref<Framebuffer>> m_Framebuffers;
    VkRenderPass                  m_RenderPass;
    std::vector<VkImage>          m_SwapchainImages;
    std::vector<VkImageView>      m_ImageViews;
    VkFormat                      m_ImageFormat;
    uint32_t                      m_ImageCount;

    // Misc.
    VkFormat         m_SwapchainImageFormat;
    VkPresentModeKHR m_PresentMode;

    std::shared_ptr<PhysicalDevice> _PhysicalDevice;
    std::shared_ptr<LogicalDevice>  _Device;
    std::shared_ptr<Surface>        _Surface;
    QueueFamilyIndices              _Queues;
};
