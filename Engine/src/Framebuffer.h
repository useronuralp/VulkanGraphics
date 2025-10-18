#pragma once
#include "core.h"
#include "vulkan/vulkan.h"

#include <vector>
class Framebuffer
{
   public:
    Framebuffer() = default;
    Framebuffer(
        const VkRenderPass&      InRenderPass,
        std::vector<VkImageView> InAtachments,
        uint32_t                 InWidth,
        uint32_t                 InHeight,
        int                      InLayerCount = 1);
    ~Framebuffer();
    const uint32_t&      GetWidth();
    const uint32_t&      GetHeight();
    const VkFramebuffer& GetHandle();

   private:
    VkFramebuffer _Framebuffer = VK_NULL_HANDLE;
    uint32_t      _Width;
    uint32_t      _Height;
};
