#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.h>

class Framebuffer;
class VulkanContext;

class RenderPass
{
   public:
    struct AttachmentInfo
    {
        VkFormat            Format;
        VkImageLayout       FinalLayout;
        VkAttachmentLoadOp  LoadOp;
        VkAttachmentStoreOp StoreOp;
        VkClearValue        ClearValue;
    };

    struct CreateInfo
    {
        std::vector<AttachmentInfo>      Attachments;
        std::vector<VkSubpassDependency> Dependencies;
        bool                             HasDepth = false;
        std::string                      DebugName;
    };

   public:
    RenderPass() = default;
    ~RenderPass();
    RenderPass(VulkanContext& InContext, const CreateInfo& InInfo);
    void Begin(VkCommandBuffer InCmdBuffer, Framebuffer& InFramebuffer, const char* InDebugName = nullptr);
    void End(VkCommandBuffer InCmdBuffer);

    VkRenderPass GetHandle() const;

   private:
    void CreateRenderPass();
    void Destroy();

   private:
    VulkanContext& _Context;
    CreateInfo     _Info;
    VkRenderPass   _RenderPass = VK_NULL_HANDLE;
    std::string    _DebugName;
};

struct RDGScopedDebugLabel
{
    VkCommandBuffer                cmd{};
    PFN_vkCmdEndDebugUtilsLabelEXT funcEndLabel{};

    RDGScopedDebugLabel(VkCommandBuffer command, VkDevice device, const char* name) : cmd(command)
    {
        VkDebugUtilsLabelEXT label{};
        label.sType         = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName    = name;
        label.color[0]      = 0.0f;
        label.color[1]      = 1.0f;
        label.color[2]      = 0.0f;
        label.color[3]      = 1.0f;

        auto funcBeginLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdBeginDebugUtilsLabelEXT");
        funcEndLabel        = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device, "vkCmdEndDebugUtilsLabelEXT");

        if (funcBeginLabel)
            funcBeginLabel(cmd, &label);
    }

    ~RDGScopedDebugLabel()
    {
        if (funcEndLabel)
            funcEndLabel(cmd);
    }
};

// ---------------------------
// Resource Representation
// ---------------------------
struct RGResource
{
    std::string   Name;
    VkImage       Image  = VK_NULL_HANDLE; // optional: existing image
    VkFormat      Format = VK_FORMAT_UNDEFINED;
    VkExtent2D    Extent{};
    VkImageLayout CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool          IsTransient   = true;

    // In a full system this would hold allocation info
};

// ---------------------------
// Pass Representation
// ---------------------------
struct RGPass
{
    std::string                          Name;
    std::vector<RGResource*>             Reads;
    std::vector<RGResource*>             Writes;
    std::function<void(VkCommandBuffer)> RecordFunc;

    bool IsAsyncCompute = false; // future proofing
};

// ---------------------------
// Render Graph
// ---------------------------
class RenderGraph
{
   public:
    explicit RenderGraph(VulkanContext& InContext);

    RGResource* CreateTexture(const std::string& InName, VkImage InImage, VkFormat InFormat, uint32_t InWidth = 0, uint32_t InHeight = 0, bool InTransient = true);

    RGPass* AddPass(const std::string& InName, std::function<void(RGPass&)> InSetup);

    void Compile(); // dependency analysis / sorting
    void Execute(VkCommandBuffer InCmd);

    RGResource* GetResource(const std::string& InName);

    void DebugPrint();

    void Clear();

   private:
    VulkanContext&                               _Context;
    std::vector<std::unique_ptr<RGResource>>     _Resources;
    std::vector<std::unique_ptr<RGPass>>         _Passes;
    std::unordered_map<std::string, RGResource*> _ResourceMap;
};
