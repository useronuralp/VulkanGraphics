#pragma once
#include "core.h"
// External
#include "vulkan/vulkan.h"

#define BLUR_PASS_COUNT 6
namespace OVK
{
    class Framebuffer;
    class Image;
    class Pipeline;
    class DescriptorLayout;
    class DescriptorPool;
	class Bloom
	{
	public:
		Bloom();
        ~Bloom();
    public:
        void ApplyBloom(const VkCommandBuffer& cmdBuffer);
        void ConnectImageResourceToAddBloom(const Ref<Image>& frame);
        Ref<Image> GetPostProcessedImage() { return m_MergeColorBuffer; }
	private:

        Ref<Image>              m_HDRImage = nullptr;

        Ref<DescriptorLayout>   m_TwoSamplerLayout;
        Unique<DescriptorPool>  m_DescriptorPool;
        Ref<DescriptorLayout>   m_OneSamplerLayout;
        bool                    m_FirstPassEver = true;


        // Brigtness filtering resources.
        VkRenderPass            m_BrightnessIsolationPass;
        Ref<Image>              m_BrightnessIsolatedImage;
        Unique<Framebuffer>     m_BrightnessIsolatedFramebuffer;
        Ref<Pipeline>           m_BrightnessFilterPipeline;
        VkDescriptorSet         m_BrigtnessFilterDescriptorSet;
        VkSampler               m_BrigtnessFilterSampler;
        VkRenderPassBeginInfo   m_BrightnessFilterRenderPassBeginInfo;

        // Blur downscaling resources.
        Unique<Framebuffer>     m_BlurFramebuffers[BLUR_PASS_COUNT];
        VkRenderPass            m_BlurRenderPass;
        Ref<Image>              m_BlurColorBuffers[BLUR_PASS_COUNT];
        VkRenderPassBeginInfo   m_BlurRenderPassBeginInfo;
        VkDescriptorSet         m_BlurDescriptorSets[BLUR_PASS_COUNT];
        Ref<Pipeline>           m_BlurPipelines[BLUR_PASS_COUNT];
        VkSampler               m_BlurSamplers[BLUR_PASS_COUNT];

        // Blur upscaling resources.
        Ref<Pipeline>           m_UpscalingPipelines[BLUR_PASS_COUNT];
        VkRenderPassBeginInfo   m_UpscalingRenderPassBeginInfo;
        Ref<Image>              m_UpscalingColorBuffers[BLUR_PASS_COUNT];
        Unique<Framebuffer>     m_UpscalingFramebuffers[BLUR_PASS_COUNT];
        VkDescriptorSet         m_UpscalingDescriptorSets[BLUR_PASS_COUNT];
        VkSampler               m_UpscalingSamplersFirst[BLUR_PASS_COUNT];
        VkSampler               m_UpscalingSamplersSecond[BLUR_PASS_COUNT];

        // Merge pass resources.
        VkSampler               m_MergeSamplerBloom;
        VkSampler               m_MergeSamplerHDR;
        VkDescriptorSet         m_MergeDescriptorSet;
        Ref<Pipeline>           m_MergePipeline;
        VkRenderPass            m_MergeRenderPass;
        Unique<Framebuffer>     m_MergeFramebuffer;
        VkRenderPassBeginInfo   m_MergeRenderPassBeginInfo;
        Ref<Image>              m_MergeColorBuffer = nullptr;


    private:
        void CreateRenderPasses();
        void CreateFramebuffers();
        void SetupDesciptorSets();
        void SetupPipelines();
	};
}