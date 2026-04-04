#include "Bloom.h"
#include "CloudPass.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Image.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"
// #include "VulkanApplication.h"
//  External
#include <array>
Bloom::Bloom()
{
    CreateRenderPasses();
    CreateFramebuffers();

    std::vector<DescriptorBinding> layout{
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<DescriptorBinding> layout2{
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<DescriptorBinding> layout3{
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<VkDescriptorType> types;
    types.clear();
    types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    m_DescriptorPool = std::make_unique<DescriptorPool>(200, types);

    // Create the layouts used in the blur passes and merge.
    m_TwoSamplerLayout   = std::make_unique<DescriptorSetLayout>(layout);
    m_OneSamplerLayout   = std::make_unique<DescriptorSetLayout>(layout2);
    m_ThreeSamplerLayout = std::make_unique<DescriptorSetLayout>(layout3);

    SetupPipelines();

    SetupDesciptorSets();
}

Bloom::~Bloom()
{
    vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BrigtnessFilterSampler, nullptr);
    vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_MergeSamplerHDR, nullptr);
    vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_MergeSamplerBloom, nullptr);

    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_UpscalingSamplersSecond[i], nullptr);
        vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_UpscalingSamplersFirst[i], nullptr);
        vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BlurSamplers[i], nullptr);
    }

    vkDestroyRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_MergeRenderPass, nullptr);
    vkDestroyRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BlurRenderPass, nullptr);
    vkDestroyRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BrightnessIsolationPass, nullptr);
}

void Bloom::ConnectImageResourceToAddBloomTo(const Ref<Image>& frame, Ref<CloudPass> cloudpass)
{
    m_HDRImage = frame;
    if (m_FirstPassEver)
    {
        m_FirstPassEver = false;
        m_BrigtnessFilterSampler =
            Utils::CreateSampler(m_HDRImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    }
    else
    {
        vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), m_BrigtnessFilterSampler, nullptr);
        m_BrigtnessFilterSampler =
            Utils::CreateSampler(m_HDRImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    }
    Utils::UpdateDescriptorSet(m_BrigtnessFilterDescriptorSet, m_BrigtnessFilterSampler, m_HDRImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Merge pass.
    m_MergeSamplerHDR   = Utils::CreateSampler(m_HDRImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    m_MergeSamplerBloom = Utils::CreateSampler(
        m_UpscalingColorBuffers[BLUR_PASS_COUNT - 1], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);

    Utils::UpdateDescriptorSet(m_MergeDescriptorSet, m_MergeSamplerHDR, m_HDRImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    Utils::UpdateDescriptorSet(
        m_MergeDescriptorSet, m_MergeSamplerBloom, m_UpscalingColorBuffers[BLUR_PASS_COUNT - 1]->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // CLOUD STUFF
    cloudSampler = Utils::CreateSampler(ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);

    Utils::UpdateDescriptorSet(m_MergeDescriptorSet, cloudSampler, cloudpass->GetOutputColorView(), 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void Bloom::ApplyBloom(const VkCommandBuffer& cmdBuffer)
{
    VkDebugUtilsLabelEXT label{};
    label.sType         = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName    = "[Bloom] Brightness Filter";
    label.color[0]      = 0.0f;
    label.color[1]      = 1.0f;
    label.color[2]      = 0.0f;
    label.color[3]      = 1.0f;

    auto funcBeginLabel = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(EngineInternal::GetContext().GetDevice()->GetVKDevice(), "vkCmdBeginDebugUtilsLabelEXT");
    auto funcEndLabel   = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(EngineInternal::GetContext().GetDevice()->GetVKDevice(), "vkCmdEndDebugUtilsLabelEXT");

    VkClearValue clearValues                                       = { 0.8f, 0.1f, 0.1f, 1.0f };

    m_BrightnessFilterRenderPassBeginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    m_BrightnessFilterRenderPassBeginInfo.framebuffer              = m_BrightnessIsolatedFramebuffer->GetHandle();
    m_BrightnessFilterRenderPassBeginInfo.clearValueCount          = 1;
    m_BrightnessFilterRenderPassBeginInfo.pClearValues             = &clearValues;
    m_BrightnessFilterRenderPassBeginInfo.pNext                    = nullptr;
    m_BrightnessFilterRenderPassBeginInfo.renderPass               = m_BrightnessIsolationPass;
    m_BrightnessFilterRenderPassBeginInfo.renderArea.offset        = { 0, 0 };
    m_BrightnessFilterRenderPassBeginInfo.renderArea.extent.height = m_BrightnessIsolatedFramebuffer->GetHeight();
    m_BrightnessFilterRenderPassBeginInfo.renderArea.extent.width  = m_BrightnessIsolatedFramebuffer->GetWidth();

    CommandBuffer::BeginRenderPass(cmdBuffer, m_BrightnessFilterRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    funcBeginLabel(cmdBuffer, &label);
    CommandBuffer::BindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BrightnessFilterPipeline);
    vkCmdBindDescriptorSets(
        cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BrightnessFilterPipeline->GetPipelineLayout(), 0, 1, &m_BrigtnessFilterDescriptorSet, 0, nullptr);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    funcEndLabel(cmdBuffer);
    CommandBuffer::EndRenderPass(cmdBuffer);

    VkDeviceSize offset = { 0 };
    // Blur pass.
    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        clearValues = { 0.8f, 0.1f, 0.1f, 1.0f };

        // Downscaling pass
        m_BlurRenderPassBeginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        m_BlurRenderPassBeginInfo.framebuffer              = m_BlurFramebuffers[i]->GetHandle();
        m_BlurRenderPassBeginInfo.clearValueCount          = 1;
        m_BlurRenderPassBeginInfo.pClearValues             = &clearValues;
        m_BlurRenderPassBeginInfo.pNext                    = nullptr;
        m_BlurRenderPassBeginInfo.renderPass               = m_BlurRenderPass;
        m_BlurRenderPassBeginInfo.renderArea.offset        = { 0, 0 };
        m_BlurRenderPassBeginInfo.renderArea.extent.height = m_BlurFramebuffers[i]->GetHeight();
        m_BlurRenderPassBeginInfo.renderArea.extent.width  = m_BlurFramebuffers[i]->GetWidth();

        label.pLabelName                                   = "[Bloom] Downscaling";

        CommandBuffer::BeginRenderPass(cmdBuffer, m_BlurRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        funcBeginLabel(cmdBuffer, &label);
        CommandBuffer::BindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BlurPipelines[i]);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BlurPipelines[i]->GetPipelineLayout(), 0, 1, &m_BlurDescriptorSets[i], 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        funcEndLabel(cmdBuffer);
        CommandBuffer::EndRenderPass(cmdBuffer);
    }

    // Upscaling pass.
    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        label.pLabelName                                        = "[Bloom] Upscaling";
        clearValues                                             = { 0.8f, 0.1f, 0.1f, 1.0f };

        m_UpscalingRenderPassBeginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        m_UpscalingRenderPassBeginInfo.framebuffer              = m_UpscalingFramebuffers[i]->GetHandle();
        m_UpscalingRenderPassBeginInfo.clearValueCount          = 1;
        m_UpscalingRenderPassBeginInfo.pClearValues             = &clearValues;
        m_UpscalingRenderPassBeginInfo.pNext                    = nullptr;
        m_UpscalingRenderPassBeginInfo.renderPass               = m_BlurRenderPass;
        m_UpscalingRenderPassBeginInfo.renderArea.offset        = { 0, 0 };
        m_UpscalingRenderPassBeginInfo.renderArea.extent.height = m_UpscalingFramebuffers[i]->GetHeight();
        m_UpscalingRenderPassBeginInfo.renderArea.extent.width  = m_UpscalingFramebuffers[i]->GetWidth();

        CommandBuffer::BeginRenderPass(cmdBuffer, m_UpscalingRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        funcBeginLabel(cmdBuffer, &label);
        CommandBuffer::BindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UpscalingPipelines[i]);
        vkCmdBindDescriptorSets(
            cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_UpscalingPipelines[i]->GetPipelineLayout(), 0, 1, &m_UpscalingDescriptorSets[i], 0, nullptr);
        vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
        funcEndLabel(cmdBuffer);
        CommandBuffer::EndRenderPass(cmdBuffer);
    }

    clearValues                                         = { 0.8f, 0.1f, 0.1f, 1.0f };

    m_MergeRenderPassBeginInfo.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    m_MergeRenderPassBeginInfo.framebuffer              = m_MergeFramebuffer->GetHandle();
    m_MergeRenderPassBeginInfo.clearValueCount          = 1;
    m_MergeRenderPassBeginInfo.pClearValues             = &clearValues;
    m_MergeRenderPassBeginInfo.pNext                    = nullptr;
    m_MergeRenderPassBeginInfo.renderPass               = m_MergeRenderPass;
    m_MergeRenderPassBeginInfo.renderArea.offset        = { 0, 0 };
    m_MergeRenderPassBeginInfo.renderArea.extent.height = m_MergeFramebuffer->GetHeight();
    m_MergeRenderPassBeginInfo.renderArea.extent.width  = m_MergeFramebuffer->GetWidth();

    label.pLabelName                                    = "[Bloom] Merge";

    CommandBuffer::BeginRenderPass(cmdBuffer, m_MergeRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    funcBeginLabel(cmdBuffer, &label);
    CommandBuffer::BindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MergePipeline);
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_MergePipeline->GetPipelineLayout(), 0, 1, &m_MergeDescriptorSet, 0, nullptr);
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
    funcEndLabel(cmdBuffer);
    CommandBuffer::EndRenderPass(cmdBuffer);
}

void Bloom::CreateRenderPasses()
{
    // Isolating the bright parts render pass.
    VkAttachmentDescription isolationColorAttachmentDescription;
    VkAttachmentReference   isolationColorAttachmentRef;

    isolationColorAttachmentDescription.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    isolationColorAttachmentDescription.samples        = VK_SAMPLE_COUNT_1_BIT;
    isolationColorAttachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    isolationColorAttachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    isolationColorAttachmentDescription.flags          = 0;
    isolationColorAttachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    isolationColorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    isolationColorAttachmentDescription.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    isolationColorAttachmentDescription.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    isolationColorAttachmentRef.attachment             = 0;
    isolationColorAttachmentRef.layout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription isolationSubpass{};
    isolationSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    isolationSubpass.colorAttachmentCount = 1;
    isolationSubpass.pColorAttachments    = &isolationColorAttachmentRef;

    VkSubpassDependency isolationDependency{};
    isolationDependency.srcSubpass    = 0;
    isolationDependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    isolationDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    isolationDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    isolationDependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    isolationDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 1> isolationAttachmentDescriptions;
    isolationAttachmentDescriptions[0] = isolationColorAttachmentDescription;

    VkRenderPassCreateInfo isolationRenderPassInfo{};
    isolationRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    isolationRenderPassInfo.attachmentCount = static_cast<uint32_t>(isolationAttachmentDescriptions.size()); // Number of attachments.
    isolationRenderPassInfo.pAttachments    = isolationAttachmentDescriptions.data(); // An array with the size of "attachmentCount".
    isolationRenderPassInfo.subpassCount    = 1;
    isolationRenderPassInfo.pSubpasses      = &isolationSubpass;
    isolationRenderPassInfo.dependencyCount = 1;
    isolationRenderPassInfo.pDependencies   = &isolationDependency;

    ENSURE(
        vkCreateRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &isolationRenderPassInfo, nullptr, &m_BrightnessIsolationPass) == VK_SUCCESS,
        "Failed to create a render pass.");

    // Blur render pass.
    VkAttachmentDescription blurColorAttachmentDescription;
    VkAttachmentReference   blurColorAttachmentRef;

    blurColorAttachmentDescription.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    blurColorAttachmentDescription.samples        = VK_SAMPLE_COUNT_1_BIT;
    blurColorAttachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    blurColorAttachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    blurColorAttachmentDescription.flags          = 0;
    blurColorAttachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    blurColorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    blurColorAttachmentDescription.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    blurColorAttachmentDescription.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    blurColorAttachmentRef.attachment             = 0;
    blurColorAttachmentRef.layout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription blurSubpass{};
    blurSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    blurSubpass.colorAttachmentCount = 1;
    blurSubpass.pColorAttachments    = &blurColorAttachmentRef;

    VkSubpassDependency blurDependency{};
    blurDependency.srcSubpass    = 0;
    blurDependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    blurDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    blurDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    blurDependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    blurDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 1> blurAttachmentDescriptions;
    blurAttachmentDescriptions[0] = blurColorAttachmentDescription;

    VkRenderPassCreateInfo blurRenderPassInfo{};
    blurRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    blurRenderPassInfo.attachmentCount = static_cast<uint32_t>(blurAttachmentDescriptions.size()); // Number of attachments.
    blurRenderPassInfo.pAttachments    = blurAttachmentDescriptions.data(); // An array with the size of "attachmentCount".
    blurRenderPassInfo.subpassCount    = 1;
    blurRenderPassInfo.pSubpasses      = &blurSubpass;
    blurRenderPassInfo.dependencyCount = 1;
    blurRenderPassInfo.pDependencies   = &blurDependency;

    ENSURE(
        vkCreateRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &blurRenderPassInfo, nullptr, &m_BlurRenderPass) == VK_SUCCESS,
        "Failed to create a render pass.");

    // Merge render pass. // TO DO: This is same as the above render pass. Merge
    // these two together.
    VkAttachmentDescription mergeColorAttachmentDescription;
    VkAttachmentReference   mergeColorAttachmentRef;

    mergeColorAttachmentDescription.format         = VK_FORMAT_R16G16B16A16_SFLOAT;
    mergeColorAttachmentDescription.samples        = VK_SAMPLE_COUNT_1_BIT;
    mergeColorAttachmentDescription.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    mergeColorAttachmentDescription.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    mergeColorAttachmentDescription.flags          = 0;
    mergeColorAttachmentDescription.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    mergeColorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    mergeColorAttachmentDescription.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    mergeColorAttachmentDescription.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    mergeColorAttachmentRef.attachment             = 0;
    mergeColorAttachmentRef.layout                 = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription mergeSubpass{};
    mergeSubpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
    mergeSubpass.colorAttachmentCount = 1;
    mergeSubpass.pColorAttachments    = &mergeColorAttachmentRef;

    VkSubpassDependency mergeDependency{};
    mergeDependency.srcSubpass    = 0;
    mergeDependency.dstSubpass    = VK_SUBPASS_EXTERNAL;
    mergeDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    mergeDependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    mergeDependency.dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    mergeDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    std::array<VkAttachmentDescription, 1> mergeAttachmentDescriptions;
    mergeAttachmentDescriptions[0] = mergeColorAttachmentDescription;

    VkRenderPassCreateInfo mergeRenderPassInfo{};
    mergeRenderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    mergeRenderPassInfo.attachmentCount = static_cast<uint32_t>(mergeAttachmentDescriptions.size()); // Number of attachments.
    mergeRenderPassInfo.pAttachments    = mergeAttachmentDescriptions.data(); // An array with the size of "attachmentCount".
    mergeRenderPassInfo.subpassCount    = 1;
    mergeRenderPassInfo.pSubpasses      = &mergeSubpass;
    mergeRenderPassInfo.dependencyCount = 1;
    mergeRenderPassInfo.pDependencies   = &mergeDependency;

    ENSURE(
        vkCreateRenderPass(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &mergeRenderPassInfo, nullptr, &m_MergeRenderPass) == VK_SUCCESS,
        "Failed to create a render pass.");
}

void Bloom::CreateFramebuffers()
{
    m_BrightnessIsolatedImage = std::make_unique<Image>(
        EngineInternal::GetContext().GetSurface()->GetVKExtent().width,
        EngineInternal::GetContext().GetSurface()->GetVKExtent().height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ImageType::COLOR);

    std::vector<VkImageView> attachments = {
        m_BrightnessIsolatedImage->GetImageView(),
    };

    m_BrightnessIsolatedFramebuffer = std::make_unique<Framebuffer>(
        m_BrightnessIsolationPass,
        attachments,
        EngineInternal::GetContext().GetSurface()->GetVKExtent().width,
        EngineInternal::GetContext().GetSurface()->GetVKExtent().height);

    // Setup the blur pass framebuffers.
    uint32_t width  = EngineInternal::GetContext().GetSurface()->GetVKExtent().width;
    uint32_t height = EngineInternal::GetContext().GetSurface()->GetVKExtent().height;
    // TO DO: make the iteration count n - 1 as the first iteration will use the
    // image rendered by the HDR render pass.

    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        height /= 2;
        width /= 2;

        m_BlurColorBuffers[i] =
            std::make_unique<Image>(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, ImageType::COLOR);

        attachments           = { m_BlurColorBuffers[i]->GetImageView() };

        m_BlurFramebuffers[i] = std::make_unique<Framebuffer>(m_BlurRenderPass, attachments, width, height);
    }

    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        height *= 2;
        width *= 2;

        m_UpscalingColorBuffers[i] =
            make_s<Image>(width, height, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, ImageType::COLOR);

        attachments                = { m_UpscalingColorBuffers[i]->GetImageView() };

        m_UpscalingFramebuffers[i] = std::make_unique<Framebuffer>(m_BlurRenderPass, attachments, width, height);
    }

    m_MergeColorBuffer = make_s<Image>(
        EngineInternal::GetContext().GetSurface()->GetVKExtent().width,
        EngineInternal::GetContext().GetSurface()->GetVKExtent().height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ImageType::COLOR);

    // Merge framebuffer.
    attachments        = { m_MergeColorBuffer->GetImageView() };

    m_MergeFramebuffer = std::make_unique<Framebuffer>(
        m_MergeRenderPass, attachments, EngineInternal::GetContext().GetSurface()->GetVKExtent().width, EngineInternal::GetContext().GetSurface()->GetVKExtent().height);
}

void Bloom::SetupDesciptorSets()
{
    // Brightness filter descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_DescriptorPool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_OneSamplerLayout->GetDescriptorLayout();

    VkResult rslt                = vkAllocateDescriptorSets(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_BrigtnessFilterDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    // Merge descriptor set
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = m_DescriptorPool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &m_ThreeSamplerLayout->GetDescriptorLayout();

    rslt                         = vkAllocateDescriptorSets(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_MergeDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        // Downscaling descs.
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_DescriptorPool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_OneSamplerLayout->GetDescriptorLayout();

        VkResult rslt                = vkAllocateDescriptorSets(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_BlurDescriptorSets[i]);
        ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

        // Upscaling descs.
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = m_DescriptorPool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &m_TwoSamplerLayout->GetDescriptorLayout();

        rslt                         = vkAllocateDescriptorSets(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_UpscalingDescriptorSets[i]);
        ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");
    }

    // Allocate blur descriptor Sets.
    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        if (i == 0)
        {
            m_BlurSamplers[i] =
                Utils::CreateSampler(m_BrightnessIsolatedImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
            Utils::UpdateDescriptorSet(
                m_BlurDescriptorSets[i], m_BlurSamplers[i], m_BrightnessIsolatedImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else
        {
            m_BlurSamplers[i] =
                Utils::CreateSampler(m_BlurColorBuffers[i - 1], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
            Utils::UpdateDescriptorSet(
                m_BlurDescriptorSets[i], m_BlurSamplers[i], m_BlurColorBuffers[i - 1]->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    int a = BLUR_PASS_COUNT - 1;
    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        if (i == 0)
        {
            // Grab the last two results.
            m_UpscalingSamplersFirst[i] =
                Utils::CreateSampler(m_BlurColorBuffers[a], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
            m_UpscalingSamplersSecond[i] =
                Utils::CreateSampler(m_BlurColorBuffers[a - 1], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);

            Utils::UpdateDescriptorSet(
                m_UpscalingDescriptorSets[i], m_UpscalingSamplersFirst[i], m_BlurColorBuffers[a]->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            Utils::UpdateDescriptorSet(
                m_UpscalingDescriptorSets[i], m_UpscalingSamplersSecond[i], m_BlurColorBuffers[a - 1]->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
        else
        {
            m_UpscalingSamplersFirst[i] = Utils::CreateSampler(
                m_UpscalingColorBuffers[i - 1], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
            Utils::UpdateDescriptorSet(
                m_UpscalingDescriptorSets[i], m_UpscalingSamplersFirst[i], m_UpscalingColorBuffers[i - 1]->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            if (a == 0)
            {
                m_UpscalingSamplersSecond[i] = Utils::CreateSampler(
                    m_BrightnessIsolatedImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
                Utils::UpdateDescriptorSet(
                    m_UpscalingDescriptorSets[i], m_UpscalingSamplersSecond[i], m_BrightnessIsolatedImage->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            else
            {
                m_UpscalingSamplersSecond[i] = Utils::CreateSampler(
                    m_BlurColorBuffers[a - 1], ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
                Utils::UpdateDescriptorSet(
                    m_UpscalingDescriptorSets[i], m_UpscalingSamplersSecond[i], m_BlurColorBuffers[a - 1]->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        }
        a--;
    }
}

void Bloom::SetupPipelines()
{
    auto& context              = EngineInternal::GetContext();
    m_BrightnessFilterPipeline = PipelineBuilder(context)
                                     .SetRenderPass(m_BrightnessIsolationPass)
                                     .SetDescriptorSetLayout(m_OneSamplerLayout)
                                     .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                                     .SetFragmentShader("assets/shaders/brightnessFilterFRAG.spv")
                                     .SetDepthTest(VK_FALSE)
                                     .SetDepthWrite(VK_FALSE)
                                     .SetFixedViewport(m_BrightnessIsolatedFramebuffer->GetWidth(), m_BrightnessIsolatedFramebuffer->GetHeight())
                                     .SetDynamicStatesEnabled(false)
                                     .Build();

    for (int i = 0; i < BLUR_PASS_COUNT; i++)
    {
        m_BlurPipelines[i] = PipelineBuilder(context)
                                 .SetRenderPass(m_BlurRenderPass)
                                 .SetDescriptorSetLayout(m_OneSamplerLayout)
                                 .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                                 .SetFragmentShader("assets/shaders/blurShaderFRAG.spv")
                                 .SetDepthTest(VK_FALSE)
                                 .SetDepthWrite(VK_FALSE)
                                 .SetFixedViewport(m_BlurFramebuffers[i]->GetWidth(), m_BlurFramebuffers[i]->GetHeight())
                                 .SetDynamicStatesEnabled(false)
                                 .Build();

        m_UpscalingPipelines[i] = PipelineBuilder(context)
                                      .SetRenderPass(m_BlurRenderPass)
                                      .SetDescriptorSetLayout(m_TwoSamplerLayout)
                                      .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                                      .SetFragmentShader("assets/shaders/upscaleShaderFRAG.spv")
                                      .SetDepthTest(VK_FALSE)
                                      .SetDepthWrite(VK_FALSE)
                                      .SetFixedViewport(m_UpscalingFramebuffers[i]->GetWidth(), m_UpscalingFramebuffers[i]->GetHeight())
                                      .SetDynamicStatesEnabled(false)
                                      .Build();
    }

    m_MergePipeline = PipelineBuilder(context)
                          .SetRenderPass(m_MergeRenderPass)
                          .SetDescriptorSetLayout(m_ThreeSamplerLayout)
                          .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                          .SetFragmentShader("assets/shaders/finalPassShaderFRAG.spv")
                          .SetDepthTest(VK_FALSE)
                          .SetDepthWrite(VK_FALSE)
                          .SetFixedViewport(m_MergeFramebuffer->GetWidth(), m_MergeFramebuffer->GetHeight())
                          .SetDynamicStatesEnabled(false)
                          .Build();
}
