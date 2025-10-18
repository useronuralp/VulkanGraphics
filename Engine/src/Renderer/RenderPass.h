#pragma once
#include "VulkanContext.h"

#include <iostream>
#include <vector>

class RenderPass {
   public:
    struct AttachmentInfo {
        VkFormat            Format;
        VkImageLayout       FinalLayout;
        VkAttachmentLoadOp  LoadOp;
        VkAttachmentStoreOp StoreOp;
        VkClearValue        ClearValue;
    };

    struct CreateInfo {
        std::vector<AttachmentInfo>      Attachments;
        std::vector<VkSubpassDependency> Dependencies;
        bool                             HasDepth = false;
        std::string                      DebugName;
    };

   public:
    RenderPass() = default;
    RenderPass(VulkanContext& InContext, const CreateInfo& InInfo);
    ~RenderPass();

    void Begin(VkCommandBuffer InCmdBuffer, Framebuffer& InFramebuffer);
    void End(VkCommandBuffer InCmdBuffer);

    VkRenderPass GetHandle() const;

   private:
    void CreateRenderPass();
    void Destroy();

   private:
    VulkanContext& _Context;
    CreateInfo     _Info;
    VkRenderPass   _RenderPass = VK_NULL_HANDLE;
};
