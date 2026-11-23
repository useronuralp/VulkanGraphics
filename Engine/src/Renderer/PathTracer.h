#pragma once
#include "RayTracingFunctions.h"
#include "Renderer.h"

#include <cstddef>
#include <vulkan/vulkan.h>

class ComputePipeline;
class DescriptorPool;
class DescriptorSetLayout;
class Image;
class Pipeline;

// -------------------------------------------------------------------------------------------------
// VulkanPathTracer
// -------------------------------------------------------------------------------------------------
// Frame structure (one command buffer):
//
//   [Trace pass]   writes linear HDR radiance into _OutputImage (RGBA32F storage image, GENERAL layout)
//        |           Now: compute reference path tracer over an analytic Cornell box (ptCornell.comp),
//        |           1 sample per pixel per frame, blended into the running average already in the image.
//        |           Later: vkCmdTraceRaysKHR against the scene's acceleration structures.
//        |  barrier: trace writes -> fragment shader reads  (read-after-write)
//        v
//   [Display pass] swapchain render pass: fullscreen triangle reads _OutputImage, applies
//                  exposure + tonemap + sRGB encode, then ImGui draws on top.
//
// Swapchain acquire / submit / present / resize are inherited from VulkanRendererBase.
// -------------------------------------------------------------------------------------------------
class VulkanPathTracer : public VulkanRendererBase
{
   public:
    VulkanPathTracer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera);
    ~VulkanPathTracer() override; // out-of-line: base holds Unique<RenderPass>, which must be complete where it's destroyed

    // RendererInterface
    void Init() override;
    void RenderFrame(float InDeltaTime) override;
    void Cleanup() override;
    void SetScene(const Ref<Scene>& InScene) override;

    RendererType GetType() const final
    {
        return RendererType::PathTracer;
    }

   protected:
    void OnResize() override;

   private:
    struct DisplayPushConstants
    {
        float    Exposure   = 1.0f;
        uint32_t Tonemapper = 1; // 0 = none (clamp), 1 = ACES fitted
        uint32_t EncodeSRGB = 1; // 1 if the swapchain format is UNORM and the shader must apply the sRGB curve
        uint32_t Padding    = 0;
    };
    static_assert(sizeof(DisplayPushConstants) == 16, "Must match the push_constant block in ptDisplay.frag");

    // Re-pushed every frame. Push constants rather than a uniform buffer: 96 bytes fits in the 128 the spec
    // guarantees, and they're recorded into the command buffer, so there's no buffer to create, no
    // per-frame-in-flight copies and nothing to synchronise.
    // Layout must match ptCornell.comp (std430: mat4 @0, vec2 @64, then uints @72, 76, 80).
    struct TracePushConstants
    {
        glm::mat4 CameraToWorld{ 1.0f }; // inverse(view): column 3 = camera position, upper 3x3 = camera axes
        glm::vec2 TanHalfFov{ 0.0f };    // half-extent of the image plane at distance 1 (x = horizontal, y = vertical)
        uint32_t  FrameIndex  = 0;       // seeds the RNG; never resets, so every frame draws fresh random numbers
        uint32_t  SampleCount = 0;       // samples already averaged into the image; 0 = overwrite
        uint32_t  MaxBounces  = 0;       // scattering events per path
        uint32_t  Padding[3]{};
    };
    static_assert(sizeof(TracePushConstants) == 96, "Must match the push_constant block in ptCornell.comp");
    static_assert(offsetof(TracePushConstants, TanHalfFov) == 64 && offsetof(TracePushConstants, FrameIndex) == 72 &&
                      offsetof(TracePushConstants, SampleCount) == 76 && offsetof(TracePushConstants, MaxBounces) == 80,
                  "Must match the push_constant block in ptCornell.comp");

    void QueryRayTracingProperties();
    void CreateOutputImage();
    void CreateOutputDescriptor();
    void UpdateOutputDescriptor();
    void CreateTracePass();
    void CreateDisplayPipeline();

    void RecordTracePass(VkCommandBuffer InCmd);
    void RecordDisplayPass(VkCommandBuffer InCmd);
    void DrawImGuiPanel();

    void ResetAccumulation();
    bool HasCameraMoved();

   private:
    // Extension entry points, loaded once in Init(). Private on purpose: helpers that need them are handed
    // a const& explicitly rather than reaching for them through a globally visible object like Device.
    RayTracingFunctions _RT;

    // Queried once; the SBT (Part 3) and acceleration-structure builds (Part 1/2) depend on these limits.
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR    _RTPipelineProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR _ASProperties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };

    // Output: linear HDR radiance. RGBA32F because progressive accumulation adds tiny per-frame
    // contributions to a growing average; fp16's 11-bit mantissa stops registering them after a few
    // hundred frames.
    Ref<Image> _OutputImage;
    VkExtent2D _OutputExtent{};
    bool       _OutputNeedsLayoutInit = true; // first use: UNDEFINED -> GENERAL

    // One descriptor set exposing _OutputImage to both passes: the trace pass writes it (compute), the
    // display pass reads it (fragment). Same image, same layout, so there's no reason for two sets.
    Ref<DescriptorPool>      _DescriptorPool;
    Ref<DescriptorSetLayout> _OutputSetLayout;
    VkDescriptorSet          _OutputSet = VK_NULL_HANDLE;

    // Trace pass
    Unique<ComputePipeline> _TracePass;

    // Display pass
    Ref<Pipeline>        _DisplayPipeline;
    DisplayPushConstants _DisplayParams;

    // Accumulation. _AccumulatedFrames = samples per pixel in the image right now (1 per frame); reset whenever
    // what the camera sees changes. _FrameIndex only ever counts up: it seeds the RNG, and reusing seeds after
    // a reset would replay the same noise.
    uint32_t  _AccumulatedFrames = 0;
    uint32_t  _FrameIndex        = 0;
    glm::mat4 _LastViewProjection{ 0.0f };

    // Scattering events per path, editable in ImGui. 0 = only emitters visible, 1 = direct lighting,
    // 2+ = indirect (the red/green colour bleeding). 8 is plenty for a diffuse Cornell box.
    int _MaxBounces = 8;
};
