#include "core.h"
#include "Device.h"
#include "Framebuffer.h"
#include "RenderPass.h"
#include "Surface.h"
#include "VulkanContext.h"

#include <queue>
#include <unordered_set>

void RenderPass::CreateRenderPass()
{
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference>   colorRefs;
    VkAttachmentReference                depthRef{};

    uint32_t index = 0;
    for (auto& attachment : _Info.Attachments)
    {
        VkAttachmentDescription desc{};
        desc.format         = attachment.Format;
        desc.samples        = VK_SAMPLE_COUNT_1_BIT;
        desc.loadOp         = attachment.LoadOp;
        desc.storeOp        = attachment.StoreOp;
        desc.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        desc.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        desc.finalLayout    = attachment.FinalLayout;

        attachments.push_back(desc);

        if (attachment.FinalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || attachment.FinalLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
            depthRef = { index, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        else
            colorRefs.push_back({ index, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        ++index;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = (uint32_t)colorRefs.size();
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = _Info.HasDepth ? &depthRef : nullptr;

    VkSubpassDependency defaultDependency{};
    bool                useDefaultDep = (uint32_t)_Info.Dependencies.empty();
    // TODO: Add ensure macro here
    if (useDefaultDep)
    {
        defaultDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        defaultDependency.dstSubpass    = 0;
        defaultDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.srcAccessMask = 0;
        defaultDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = (uint32_t)attachments.size();
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;

    if (useDefaultDep)
    {
        defaultDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        defaultDependency.dstSubpass = 0;

        // Wait for previous frame’s color output and/or presentation
        defaultDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.srcAccessMask = 0;

        // Synchronize layout transition -> color attachment clear/write
        defaultDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        defaultDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // If you also have depth attachments, include:
        // defaultDependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // defaultDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        // defaultDependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }
    else
    {
        rpInfo.dependencyCount = static_cast<uint32_t>(_Info.Dependencies.size());
        rpInfo.pDependencies   = _Info.Dependencies.data();
    }

    ENSURE(vkCreateRenderPass(_Context.GetDevice()->GetVKDevice(), &rpInfo, nullptr, &_RenderPass) == VK_SUCCESS, "Failed");
}
void RenderPass::Destroy()
{
    VkDevice device = _Context.GetDevice()->GetVKDevice();
    if (_RenderPass)
        vkDestroyRenderPass(device, _RenderPass, nullptr);
}

RenderPass::RenderPass(VulkanContext& InContext, const CreateInfo& InInfo) : _Context(InContext), _Info(InInfo)
{
    CreateRenderPass();
}

RenderPass::~RenderPass()
{
    Destroy();
}

void RenderPass::Begin(VkCommandBuffer InCmdBuffer, Framebuffer& InFramebuffer, const char* InDebugName /*= nullptr*/)
{
    _DebugName = InDebugName ? InDebugName : "";
    std::vector<VkClearValue> clearValues;
    for (auto& attachment : _Info.Attachments)
        clearValues.push_back(attachment.ClearValue);

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass               = _RenderPass;
    beginInfo.framebuffer              = InFramebuffer.GetHandle();
    beginInfo.renderArea.extent.height = InFramebuffer.GetHeight();
    beginInfo.renderArea.extent.width  = InFramebuffer.GetWidth();
    beginInfo.clearValueCount          = static_cast<uint32_t>(clearValues.size());
    beginInfo.pClearValues             = clearValues.data();
    beginInfo.pNext                    = nullptr;

    vkCmdBeginRenderPass(InCmdBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Automatically begin debug label if name is provided
    if (!_DebugName.empty())
    {
        VkDebugUtilsLabelEXT label{};
        label.sType         = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label.pLabelName    = InDebugName;
        label.color[0]      = 0.0f;
        label.color[1]      = 1.0f;
        label.color[2]      = 0.0f;
        label.color[3]      = 1.0f;

        auto funcBeginLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(_Context.GetDevice()->GetVKDevice(), "vkCmdBeginDebugUtilsLabelEXT");
        if (funcBeginLabel)
        {
            funcBeginLabel(InCmdBuffer, &label);
        }
    }
}

void RenderPass::End(VkCommandBuffer InCmdBuffer)
{
    // End debug label if needed
    if (!_DebugName.empty())
    {
        auto funcEndLabel = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(_Context.GetDevice()->GetVKDevice(), "vkCmdEndDebugUtilsLabelEXT");
        if (funcEndLabel)
            funcEndLabel(InCmdBuffer);
    }

    vkCmdEndRenderPass(InCmdBuffer);
}

VkRenderPass RenderPass::GetHandle() const
{
    return _RenderPass;
}

RenderGraph::RenderGraph(VulkanContext& context) : _Context(context)
{
}

// ------------------------------------------------
// Resource Creation
// ------------------------------------------------
RGResource* RenderGraph::CreateTexture(const std::string& InName, VkImage InImage, VkFormat InFormat, uint32_t InWidth, uint32_t InHeight, bool InTransient)
{
    auto res             = std::make_unique<RGResource>();
    res->Image           = InImage;
    res->Name            = InName;
    res->Format          = InFormat;
    res->IsTransient     = InTransient;
    res->Extent          = { InWidth, InHeight };

    auto ptr             = res.get();
    _ResourceMap[InName] = ptr;
    _Resources.push_back(std::move(res));
    return ptr;
}

// ------------------------------------------------
// Pass Creation
// ------------------------------------------------
RGPass* RenderGraph::AddPass(const std::string& InName, std::function<void(RGPass&)> InSetup)
{
    auto pass  = std::make_unique<RGPass>();
    pass->Name = InName;
    InSetup(*pass);
    _Passes.push_back(std::move(pass));
    return _Passes.back().get();
}

// ------------------------------------------------
// Compilation (ordering / future: barriers)
// ------------------------------------------------
void RenderGraph::Compile()
{
    /// Step 1: Build dependency map
    std::unordered_map<RGPass*, std::unordered_set<RGPass*>> dependencies;
    std::unordered_map<RGPass*, int>                         inDegree;

    // Initialize
    for (auto& pass : _Passes)
    {
        dependencies[pass.get()] = {};
        inDegree[pass.get()]     = 0;
    }

    // Fill dependencies
    for (auto& passA : _Passes)
    {
        for (auto* readRes : passA->Reads)
        {
            for (auto& passB : _Passes)
            {
                if (passB.get() == passA.get())
                    continue;

                // If passB writes the resource passA reads, passA depends on passB
                auto it = std::find(passB->Writes.begin(), passB->Writes.end(), readRes);
                if (it != passB->Writes.end())
                {
                    dependencies[passA.get()].insert(passB.get());
                }
            }
        }
    }

    // Compute in-degree for topological sort
    for (auto& [pass, deps] : dependencies)
    {
        inDegree[pass] = static_cast<int>(deps.size());
    }

    // Step 2: Kahn's algorithm for topological sort
    std::vector<std::unique_ptr<RGPass>> sortedPasses;
    std::queue<RGPass*>                  ready;

    for (auto& [pass, deg] : inDegree)
        if (deg == 0)
            ready.push(pass);

    while (!ready.empty())
    {
        RGPass* current = ready.front();
        ready.pop();

        // Move unique_ptr into sorted list
        for (auto it = _Passes.begin(); it != _Passes.end(); ++it)
        {
            if (it->get() == current)
            {
                sortedPasses.push_back(std::move(*it));
                _Passes.erase(it);
                break;
            }
        }

        // Decrease in-degree of dependent passes
        for (auto& [pass, deps] : dependencies)
        {
            if (deps.find(current) != deps.end())
            {
                deps.erase(current);
                inDegree[pass]--;
                if (inDegree[pass] == 0)
                    ready.push(pass);
            }
        }
    }

    // If _Passes is not empty, there is a cycle
    if (!_Passes.empty())
    {
        PrintError("RenderGraph::Compile() detected a cyclic dependency!");
        assert(false);
    }

    // Replace passes with sorted order
    _Passes = std::move(sortedPasses);

    PrintRenderGraph("RenderGraph compiled successfully. Pass execution order:");
    for (auto& pass : _Passes)
    {
        PrintRenderGraph(" - " + pass->Name);
    }
}

// ------------------------------------------------
// Execution
// ------------------------------------------------
void RenderGraph::Execute(VkCommandBuffer InCmd)
{
    for (auto& pass : _Passes)
    {
        if (pass->RecordFunc)
        {
            // In a future step, you’ll insert proper barriers here
            pass->RecordFunc(InCmd);
        }
    }
}

RGResource* RenderGraph::GetResource(const std::string& InName)
{
    auto it = _ResourceMap.find(InName);
    return it != _ResourceMap.end() ? it->second : nullptr;
}

void RenderGraph::Clear()
{
    _Passes.clear(); // Remove all passes
    // Optionally clear transient resources
    //_Resources.erase(std::remove_if(_Resources.begin(), _Resources.end(), [](const std::unique_ptr<RGResource>& res) { return res->IsTransient; }), _Resources.end());
    //_ResourceMap.clear();
}

void RenderGraph::DebugPrint()
{
    PrintRenderGraph("======== Render Graph Debug ========");

    for (auto& pass : _Passes)
    {
        PrintRenderGraph("Pass: " + pass->Name);

        if (!pass->Reads.empty())
        {
            std::stringstream ss;
            ss << "   Reads: ";
            for (auto* r : pass->Reads)
                ss << r->Name << ", ";
            PrintRenderGraph(ss.str());
        }

        if (!pass->Writes.empty())
        {
            std::stringstream ss;
            ss << "   Writes: ";
            for (auto* r : pass->Writes)
                ss << r->Name << ", ";
            PrintRenderGraph(ss.str());
        }
    }

    PrintRenderGraph("====================================");
}