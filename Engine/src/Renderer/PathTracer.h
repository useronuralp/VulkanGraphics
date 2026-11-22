#pragma once
#include "RayTracingFunctions.h"
#include "Renderer.h"

#include <vulkan/vulkan.h>

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
//        |           Part 0: vkCmdClearColorImage placeholder.  Part 3+: vkCmdTraceRaysKHR.
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

    void QueryRayTracingProperties();
    void CreateOutputImage();
    void CreateDisplayResources();
    void UpdateDisplayDescriptor();

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

    // Display
    Ref<DescriptorPool>      _DescriptorPool;
    Ref<DescriptorSetLayout> _DisplaySetLayout;
    VkDescriptorSet          _DisplaySet = VK_NULL_HANDLE;
    Ref<Pipeline>            _DisplayPipeline;
    DisplayPushConstants     _DisplayParams;

    // Accumulation bookkeeping (consumed by the ray-gen shader from Part 4 on)
    uint32_t  _AccumulatedFrames = 0;
    glm::mat4 _LastViewProjection{ 0.0f };

    // Part 0 only: stand-in for traced radiance, editable in ImGui. HDR so you can see the tonemapper work.
    glm::vec3 _PlaceholderRadiance{ 0.6f, 0.75f, 1.0f };
};
