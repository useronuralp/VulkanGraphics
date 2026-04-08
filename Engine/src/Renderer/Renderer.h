#pragma once

#include "core.h"

#include <chrono>
#include <imgui_impl_vulkan.h>
#include <random>

class CloudPass;
class RenderPass;
class VulkanContext;
class Swapchain;
class Pipeline;
class Framebuffer;
class Camera;
class Image;
class Model;
class ParticleSystem;
class DescriptorSetLayout;
class Bloom;
class DescriptorPool;
class Material;
class Scene;
class RenderGraph;
struct ParticleSpecs;
enum LoadingFlags;

#define MAX_POINT_LIGHT_COUNT 10
#define SHADOW_DIM            10000
#define POUNT_SHADOW_DIM      1000

class RendererInterface
{
   public:
    virtual ~RendererInterface()              = default;

    virtual void Init()                       = 0;
    virtual bool BeginFrame()                 = 0;
    virtual void RenderFrame(float DeltaTime) = 0;
    virtual void EndFrame()                   = 0;
    virtual void InitImGui()                  = 0;
    virtual void RenderImGui()                = 0;
    virtual void Cleanup()                    = 0;
};

class VulkanRenderer : public RendererInterface
{
   public:
    VulkanRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera);
    virtual ~VulkanRenderer() = default;

    // RendererInterface — shared implementations
    bool BeginFrame() override;
    void EndFrame() override;
    void InitImGui() override;
    void RenderImGui() override;
    void Cleanup() override;

    // Scene
    void       SetScene(const Ref<Scene>& InScene);
    Ref<Scene> GetScene() const;

   protected:
    void HandleWindowResize(VkResult InResult);
    void UpdateViewport_Scissor();
    void CreateSwapchainRenderPass();
    void CreateSwapchainFramebuffers();

    // Subclass must implement — called during resize
    virtual void OnResize() = 0;

    VulkanContext& _Context;
    Ref<Swapchain> _Swapchain;
    Ref<Camera>    _Camera;
    Ref<Scene>     _Scene;

    // Swapchain
    Unique<RenderPass>            _SwapchainRenderPass;
    std::vector<Ref<Framebuffer>> _SwapchainFramebuffers;
    uint32_t                      _CurrentSwapchainImageIndex = 0;

    // Viewport
    VkViewport _DynamicViewport{};
    VkRect2D   _DynamicScissor{};

    // Command buffers
    VkCommandBuffer _CmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool   _CmdPool;

    // ImGui
    VkDescriptorPool          _ImGuiPool;
    ImGui_ImplVulkan_InitInfo _ImGuiInitInfo;

    float _DeltaTime = 0.0f;
};

class VulkanForwardRenderer : public VulkanRenderer
{
   public:
    bool pointLightShadows  = true;
    bool showDOFFocus       = false;
    bool enableDepthOfField = true;

    std::random_device               rd; // obtain a random number from hardware
    std::mt19937                     gen; // seed the generator
    std::uniform_real_distribution<> distr;

    float timer                = 0.0f;
    float directionalNearPlane = 1.0f;
    float directionalFarPlane  = 100.0f;
    float pointNearPlane       = 0.0001f;
    float pointFarPlane        = 100.0f;
    int   frameCount           = 0;

    struct GlobalParametersUBO
    {
        // The alignment in a struct equals to the largest base alignemnt of any
        // of its members. In this case all of the members need to be aligned to
        // a vec4 format.
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::mat4 directionalLightMVP;
        glm::vec4 dirLightPos;
        glm::vec4 cameraPosition;
        glm::vec4 viewportDimension;
        glm::vec4 pointLightPositions[MAX_POINT_LIGHT_COUNT];
        glm::vec4 pointLightIntensities[MAX_POINT_LIGHT_COUNT];
        glm::vec4 pointLightColors[MAX_POINT_LIGHT_COUNT];
        glm::vec4 enablePointLightShadows = glm::vec4(1.0f);
        glm::vec4 directionalLightIntensity;
        glm::mat4 shadowMatrices[MAX_POINT_LIGHT_COUNT][6];
        glm::vec4 pointFarPlane;
        glm::vec4 pointLightCount;
        // padding
        float padding[12];
        // padding
        glm::vec4 DOFFramebufferSize;
        glm::vec4 cameraNearPlane;
        glm::vec4 cameraFarPlane;
        glm::vec4 showDOFFocus;
        glm::vec4 focalDepth;
        glm::vec4 focalLength;
        glm::vec4 fstop;
    };

    // todo: couple with render passes.
    //  Attachments. Each framebuffer can have multiple attachments.
    Ref<Image>              directionalShadowMapImage;
    Ref<Image>              HDRColorImage;
    Ref<Image>              HDRDepthImage;
    Ref<Image>              particleTexture;
    Ref<Image>              worleyNoiseTexture;
    Ref<Image>              dustTexture;
    Ref<Image>              fireTexture;
    std::vector<Ref<Image>> pointShadowMaps;

    // Descriptor Set Layouts
    Ref<DescriptorSetLayout> swapchainLayout;
    Ref<DescriptorSetLayout> emissiveLayout;
    Ref<DescriptorSetLayout> PBRLayout;
    Ref<DescriptorSetLayout> skyboxLayout;
    Ref<DescriptorSetLayout> cubeLayout;
    Ref<DescriptorSetLayout> particleSystemLayout;

    // Experimental
    Ref<Image>            bokehPassImage;
    VkRenderPassBeginInfo bokehPassBeginInfo;
    Unique<RenderPass>    bokehRenderPass;
    Ref<Framebuffer>      bokehPassFramebuffer;
    Ref<Pipeline>         bokehPassPipeline;

    VkRenderPassBeginInfo    bokehRenderPassBeginInfo;
    VkSampler                bokehPassSceneSampler;
    VkSampler                bokehPassDepthSampler;
    VkDescriptorSet          bokehDescriptorSet;
    Ref<DescriptorSetLayout> bokehPassLayout;

    // Pipelines
    Ref<Pipeline> EmissiveObjectPipeline;
    Ref<Pipeline> finalPassPipeline;
    Ref<Pipeline> pipeline;
    Ref<Pipeline> pointShadowPassPipeline;
    Ref<Pipeline> shadowPassPipeline;
    Ref<Pipeline> skyboxPipeline;
    Ref<Pipeline> cubePipeline;
    Ref<Pipeline> particleSystemPipeline;

    float lightFlickerRate      = 0.07f;
    float aniamtionRate         = 0.013888888f;
    int   currentAnimationFrame = 0;

    GlobalParametersUBO globalParametersUBO;
    VkBuffer            globalParametersUBOBuffer;
    VkDeviceMemory      globalParametersUBOBufferMemory;
    void*               mappedGlobalParametersModelUBOBuffer;

    Ref<Material> pbrMaterial;
    Ref<Material> emissiveMaterial;
    Ref<Material> skyboxMaterial;
    Ref<Material> cubeMaterial;

    // Others
    Ref<Bloom>      bloomAgent;
    VkSampler       finalPassSampler;
    VkDescriptorSet finalPassDescriptorSet;

    glm::mat4 directionalLightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, directionalNearPlane, directionalFarPlane);
    glm::mat4 pointLightProjectionMatrix       = glm::perspective(glm::radians(90.0f), 1.0f, pointNearPlane, pointFarPlane);

   private:
    // Framebuffer creations.
    void CreateHDRFramebuffer();
    void CreateBokehFramebuffer();

    // Pipeline creations.
    void SetupPBRPipeline();
    void SetupFinalPassPipeline();
    void SetupShadowPassPipeline();
    void SetupPointShadowPassPipeline();
    void SetupBokehPassPipeline();
    void SetupSkyboxPipeline();
    void SetupCubePipeline();
    void SetupParticleSystemPipeline();
    void SetupEmissiveObjectPipeline();

    // Render pass creations.
    void CreateBokehRenderPass();
    void CreateHDRRenderPass();
    void CreateShadowRenderPass();
    void CreatePointShadowRenderPass();

    // Specific funcs.
    void EnableDepthOfField();
    void DisableDepthOfField();
    void SyncLightsToUBO();

   public:
    VulkanForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera);

    // RendererInterface overrides
    void Init() override;
    void RenderFrame(const float InDeltaTime) override;
    void Cleanup() override;
    //  ~RendererInterface overrides

    // Model loading API
    // TODO: Better to have a ResourceLoader class that handles this, but for simplicity it's in the renderer for now.
    Ref<Model> LoadPBRModel(const std::string& InPath, LoadingFlags InFlags);
    Ref<Model> LoadSimpleModel(const std::string& InPath, LoadingFlags InFlags);
    Ref<Model> LoadSkyboxModel(const float* InVertices, uint32_t InVertexCount, Ref<Image> InCubemap);
    Ref<Model> LoadDebugModel(const float* InVertices, uint32_t InVertexCount);

    // Material access
    Ref<Material> GetPBRMaterial() const;
    Ref<Material> GetEmissiveMaterial() const;
    Ref<Material> GetSkyboxMaterial() const;
    Ref<Material> GetCubeMaterial() const;

    // Particle creation
    Ref<ParticleSystem> CreateParticleSystem(const ParticleSpecs& InSpecs, const Ref<Image>& InTexture);

   protected:
    void OnResize() override;

   private:
    Unique<RenderPass> _PointShadowRenderPass;
    Unique<RenderPass> _HDRRenderPass;
    Unique<RenderPass> _ShadowMapRenderPass;

    Ref<Framebuffer>              _DirectionalShadowMapFramebuffer;
    Ref<Framebuffer>              _HDRFramebuffer;
    std::vector<Ref<Framebuffer>> _PointShadowMapFramebuffers;

    Ref<CloudPass> _CloudPass;

    // Descriptor Pools
    Ref<DescriptorPool>                   pool;
    std::chrono::steady_clock::time_point startTime;

    Unique<RenderGraph> _Graph;

    int _FrameCount = 0;
};
