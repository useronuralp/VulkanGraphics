#include "PathTracer.h"

#include "Camera.h"
#include "CommandBuffer.h"
#include "ComputePipeline.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "DeviceRequirements.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Image.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "Renderer/RenderPass.h"
#include "Surface.h"
#include "Swapchain.h"
#include "VulkanContext.h"

#include <cmath>
#include <imgui_impl_vulkan.h>

extern FrameSync GFrameSync;

namespace
{
// How the trace pass touches _OutputImage each frame: the compute shader reads last frame's running average
// and writes the new one (imageLoad + imageStore). When the trace pass becomes vkCmdTraceRaysKHR, the stage
// becomes VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR and every barrier below follows automatically.
constexpr VkPipelineStageFlags kTraceStage       = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
constexpr VkAccessFlags        kTraceWriteAccess = VK_ACCESS_SHADER_WRITE_BIT;
constexpr VkAccessFlags        kTraceAccess      = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

// A single-image barrier. A barrier answers two questions:
//   execution: which earlier stages must finish before which later stages may start (src/dstStage)
//   memory:    which earlier writes must be made visible to which later accesses   (src/dstAccess)
// and can optionally change the image layout at the same time.
void ImageBarrier(
    VkCommandBuffer      InCmd,
    VkImage              InImage,
    VkImageLayout        InOldLayout,
    VkImageLayout        InNewLayout,
    VkPipelineStageFlags InSrcStage,
    VkAccessFlags        InSrcAccess,
    VkPipelineStageFlags InDstStage,
    VkAccessFlags        InDstAccess)
{
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout                   = InOldLayout;
    barrier.newLayout                   = InNewLayout;
    barrier.srcAccessMask               = InSrcAccess;
    barrier.dstAccessMask               = InDstAccess;
    barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                       = InImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(InCmd, InSrcStage, InDstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

bool IsSRGBFormat(VkFormat InFormat)
{
    switch (InFormat)
    {
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            return true;
        default:
            return false;
    }
}
} // namespace

VulkanPathTracer::VulkanPathTracer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
    : VulkanRendererBase(InContext, InSwapchain, InCamera)
{
}

VulkanPathTracer::~VulkanPathTracer() = default;

void VulkanPathTracer::Init()
{
    // The device enabled the RT extensions (GetDeviceRequirements); fetching their entry points is ours to do.
    _RT.Load(_Context.GetDevice()->GetVKDevice());
    QueryRayTracingProperties();
    UpdateViewport_Scissor();

    // Shared swapchain infrastructure from the base. InitImGui() (called right after Init) needs the render pass.
    CreateSwapchainRenderPass();
    CreateSwapchainFramebuffers();
    CreateCommandBuffers();

    // Small private pool. Grows new descriptor types as later parts add them (AS, buffers, textures).
    _DescriptorPool = make_s<DescriptorPool>(4, std::vector<VkDescriptorType>{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE });

    CreateOutputImage();
    CreateOutputDescriptor();
    UpdateOutputDescriptor();
    CreateTracePass();
    CreateDisplayPipeline();

    // The surface prefers R8G8B8A8_UNORM, which does NOT encode sRGB on write. If we ever end up with
    // an *_SRGB swapchain format the hardware does it for us, and doing it again would wash the image out.
    _DisplayParams.EncodeSRGB = IsSRGBFormat(_Context.GetSurface()->GetVKSurfaceFormat().format) ? 0u : 1u;
}

void VulkanPathTracer::SetScene(const Ref<Scene>& InScene)
{
    VulkanRendererBase::SetScene(InScene);

    // Part 1/2: build one BLAS per model and a TLAS over the scene's mesh objects here.
    ResetAccumulation();
}

void VulkanPathTracer::QueryRayTracingProperties()
{
    // Properties use the same pNext-chain pattern as features.
    VkPhysicalDeviceProperties2 properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
    properties.pNext            = &_RTPipelineProperties;
    _RTPipelineProperties.pNext = &_ASProperties;
    _ASProperties.pNext         = nullptr;

    vkGetPhysicalDeviceProperties2(_Context.GetPhysicalDevice()->GetVKPhysicalDevice(), &properties);

    // Don't keep pointers to a stack object around.
    _RTPipelineProperties.pNext = nullptr;
}

void VulkanPathTracer::CreateOutputImage()
{
    _OutputExtent = _Context.GetSurface()->GetVKExtent();

    // STORAGE: written by the trace pass with imageStore, read by the display pass with imageLoad.
    _OutputImage = make_s<Image>(_OutputExtent.width, _OutputExtent.height, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT, ImageType::COLOR);

    _OutputNeedsLayoutInit = true;
    ResetAccumulation();
}

void VulkanPathTracer::CreateOutputDescriptor()
{
    // Both passes touch the image 1:1 per pixel, so one storage-image descriptor serves both: the trace pass
    // writes with imageStore, the display pass reads with imageLoad. No sampler needed (and RGBA32F linear
    // filtering isn't guaranteed anyway).
    _OutputSetLayout = make_s<DescriptorSetLayout>(std::vector<DescriptorBinding>{
        { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
    });

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = _DescriptorPool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &_OutputSetLayout->GetDescriptorLayout();
    ENSURE(
        vkAllocateDescriptorSets(_Context.GetDevice()->GetVKDevice(), &allocInfo, &_OutputSet) == VK_SUCCESS,
        "Failed to allocate the path tracer output descriptor set.");
}

void VulkanPathTracer::CreateTracePass()
{
    _TracePass = make_u<ComputePipeline>(
        _Context,
        ComputePipelineDesc{
            .ShaderPath       = "assets/shaders/ptCornellCOMP.spv",
            .SetLayouts       = { _OutputSetLayout },
            .PushConstantSize = sizeof(TracePushConstants),
            .GroupSize        = { 8, 8 },
        });
}

void VulkanPathTracer::CreateDisplayPipeline()
{
    _DisplayPipeline = PipelineBuilder(_Context)
                           .SetRenderPass(_SwapchainRenderPass->GetHandle())
                           .SetDescriptorSetLayout(_OutputSetLayout)
                           .SetVertexShader("assets/shaders/ptDisplayVERT.spv")
                           .SetFragmentShader("assets/shaders/ptDisplayFRAG.spv")
                           .SetBlending(VK_FALSE)
                           .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DisplayPushConstants))
                           .Build();
}

void VulkanPathTracer::UpdateOutputDescriptor()
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView   = _OutputImage->GetImageView();
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL; // storage images must be in GENERAL when accessed
    imageInfo.sampler     = VK_NULL_HANDLE;

    VkWriteDescriptorSet write{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    write.dstSet          = _OutputSet;
    write.dstBinding      = 0;
    write.descriptorCount = 1;
    write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo      = &imageInfo;

    vkUpdateDescriptorSets(_Context.GetDevice()->GetVKDevice(), 1, &write, 0, nullptr);
}

void VulkanPathTracer::OnResize()
{
    // HandleWindowResize already waited for the device to go idle, so the old image is safe to drop.
    CreateOutputImage();
    UpdateOutputDescriptor();
    // The display pipeline uses dynamic viewport/scissor and the swapchain render pass is format-only,
    // so neither needs rebuilding.
}

void VulkanPathTracer::ResetAccumulation()
{
    _AccumulatedFrames = 0;
}

bool VulkanPathTracer::HasCameraMoved()
{
    // Exact compare is fine: with no input the camera produces bit-identical matrices frame to frame.
    const glm::mat4 viewProjection = _Camera->GetProjectionMatrix() * _Camera->GetViewMatrix();
    const bool      moved          = viewProjection != _LastViewProjection;
    _LastViewProjection            = viewProjection;
    return moved;
}

void VulkanPathTracer::RenderFrame(float InDeltaTime)
{
    _DeltaTime          = InDeltaTime;
    VkCommandBuffer cmd = _CmdBuffers[GFrameSync.CurrentBufferIndex];

    // Any change to what the camera sees invalidates the running average.
    if (HasCameraMoved())
    {
        ResetAccumulation();
    }

    CommandBuffer::BeginRecording(cmd);

    RecordTracePass(cmd);

    // Read-after-write: the display pass must see what the trace pass wrote. Unlike WAR, this needs a
    // *memory* dependency (srcAccess = the write, dstAccess = the read) so caches get flushed/invalidated.
    ImageBarrier(
        cmd,
        _OutputImage->GetVKImage(),
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_GENERAL,
        kTraceStage,
        kTraceWriteAccess,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT);

    RecordDisplayPass(cmd);

    CommandBuffer::EndRecording(cmd);

    _AccumulatedFrames++;
    _FrameIndex++;
}

void VulkanPathTracer::RecordTracePass(VkCommandBuffer InCmd)
{
    const VkImage outputImage = _OutputImage->GetVKImage();

    if (_OutputNeedsLayoutInit)
    {
        // A fresh image is in UNDEFINED. Transition once to GENERAL and keep it there forever:
        // GENERAL is the only layout that allows storage-image access, and sampling/imageLoad from it
        // is legal too, so there's no reason to bounce between layouts every frame.
        // Contents are undefined after this, which is fine: _AccumulatedFrames is 0 for a new image, so the
        // shader overwrites instead of reading.
        ImageBarrier(InCmd, outputImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, kTraceStage, kTraceAccess);
        _OutputNeedsLayoutInit = false;
    }
    else
    {
        // Accumulation made the trace pass read-modify-write, so this barrier now covers two hazards:
        //  - write-after-read against last frame's display pass (execution dependency only), and
        //  - read-after-write against last frame's trace pass: we imageLoad the average it wrote. That needs
        //    a memory dependency, so the write is in srcAccess and the read in dstAccess.
        ImageBarrier(
            InCmd,
            outputImage,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            kTraceStage | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            kTraceWriteAccess,
            kTraceStage,
            kTraceAccess);
    }

    // ── Camera -> what the shader needs to build one ray per pixel ──
    TracePushConstants params;

    // The view matrix maps world -> camera; its inverse maps camera -> world. Its columns are exactly what
    // the shader wants: column 3 is the camera position (ray origin), the upper 3x3 rotates camera-space
    // directions into world space.
    params.CameraToWorld = glm::inverse(_Camera->GetViewMatrix());

    // glm::perspective puts 1 / tan(fovY / 2) in [1][1]. Horizontal extent comes from the output image's
    // aspect rather than the camera's stored one, so pixels stay square whatever size the window is.
    const float tanHalfFovY = 1.0f / std::abs(_Camera->GetProjectionMatrix()[1][1]);
    const float aspect      = static_cast<float>(_OutputExtent.width) / static_cast<float>(_OutputExtent.height);
    params.TanHalfFov       = { tanHalfFovY * aspect, tanHalfFovY };

    // ── Accumulation state ──
    params.FrameIndex  = _FrameIndex;
    params.SampleCount = _AccumulatedFrames;
    params.MaxBounces  = static_cast<uint32_t>(_MaxBounces);

    // ── Dispatch: one invocation per pixel, in 8x8 groups ──
    _TracePass->Bind(InCmd);
    _TracePass->BindDescriptorSet(InCmd, _OutputSet);
    _TracePass->PushConstants(InCmd, params);
    _TracePass->DispatchForExtent(InCmd, _OutputExtent.width, _OutputExtent.height);
}

void VulkanPathTracer::RecordDisplayPass(VkCommandBuffer InCmd)
{
    _SwapchainRenderPass->Begin(InCmd, *_SwapchainFramebuffers[_CurrentSwapchainImageIndex], "Path Tracer Display");

    CommandBuffer::BindPipeline(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _DisplayPipeline);
    vkCmdSetViewport(InCmd, 0, 1, &_DynamicViewport);
    vkCmdSetScissor(InCmd, 0, 1, &_DynamicScissor);
    vkCmdBindDescriptorSets(InCmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _DisplayPipeline->GetPipelineLayout(), 0, 1, &_OutputSet, 0, nullptr);
    CommandBuffer::PushConstants(
        InCmd, _DisplayPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DisplayPushConstants), &_DisplayParams);
    vkCmdDraw(InCmd, 3, 1, 0, 0); // fullscreen triangle, positions generated from gl_VertexIndex

    DrawImGuiPanel();
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), InCmd);

    _SwapchainRenderPass->End(InCmd);
}

void VulkanPathTracer::DrawImGuiPanel()
{
    ImGui::Begin("Path Tracer");

    ImGui::Text("GPU: %s", _Context.GetPhysicalDevice()->GetVKProperties().deviceName);
    ImGui::Text("Output: %u x %u RGBA32F", _OutputExtent.width, _OutputExtent.height);

    if (ImGui::CollapsingHeader("Ray tracing limits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("shaderGroupHandleSize      : %u", _RTPipelineProperties.shaderGroupHandleSize);
        ImGui::Text("shaderGroupHandleAlignment : %u", _RTPipelineProperties.shaderGroupHandleAlignment);
        ImGui::Text("shaderGroupBaseAlignment   : %u", _RTPipelineProperties.shaderGroupBaseAlignment);
        ImGui::Text("maxRayRecursionDepth       : %u", _RTPipelineProperties.maxRayRecursionDepth);
        ImGui::Text("maxRayDispatchInvocations  : %u", _RTPipelineProperties.maxRayDispatchInvocationCount);
        ImGui::Text("AS scratch alignment       : %u", _ASProperties.minAccelerationStructureScratchOffsetAlignment);
        ImGui::Text("maxGeometryCount (per BLAS): %llu", static_cast<unsigned long long>(_ASProperties.maxGeometryCount));
        ImGui::Text("maxInstanceCount (per TLAS): %llu", static_cast<unsigned long long>(_ASProperties.maxInstanceCount));
    }

    if (ImGui::CollapsingHeader("Path tracing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Samples per pixel: %u", _AccumulatedFrames);

        // Changing the bounce count changes the image the average converges to, so restart it.
        if (ImGui::SliderInt("Max bounces", &_MaxBounces, 0, 16))
        {
            ResetAccumulation();
        }
        if (ImGui::Button("Reset accumulation"))
        {
            ResetAccumulation();
        }
    }

    ImGui::Separator();
    ImGui::DragFloat("Exposure", &_DisplayParams.Exposure, 0.01f, 0.0f, 32.0f);

    int         tonemapper    = static_cast<int>(_DisplayParams.Tonemapper);
    const char* tonemappers[] = { "None (clamp)", "ACES (fitted)" };
    if (ImGui::Combo("Tonemapper", &tonemapper, tonemappers, IM_ARRAYSIZE(tonemappers)))
    {
        _DisplayParams.Tonemapper = static_cast<uint32_t>(tonemapper);
    }

    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
    ImGui::End();
}

void VulkanPathTracer::Cleanup()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

    _DisplayPipeline.reset();
    _TracePass.reset();
    _OutputSetLayout.reset();
    _OutputSet = VK_NULL_HANDLE; // freed with the pool
    _DescriptorPool.reset();
    _OutputImage.reset();

    // Command buffers + ImGui
    VulkanRendererBase::Cleanup();
}
