#pragma once

#include "core.h"
#include "LightObject.h"
#include "RenderPass.h"
#include "StaticMeshObject.h"

#include <chrono>
#include <imgui_impl_vulkan.h>

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

class ForwardRenderer : public RendererInterface
{
    struct TorchFireGroup
    {
        StaticMeshObject*   Torch;
        LightObject*        Light;
        Ref<ParticleSystem> Sparks;
        Ref<ParticleSystem> Flame;
    };

   public:
    VkDescriptorPool          imguiPool;
    ImGui_ImplVulkan_InitInfo init_info;

    bool pointLightShadows  = true;
    bool showDOFFocus       = false;
    bool enableDepthOfField = true;

    std::random_device               rd; // obtain a random number from hardware
    std::mt19937                     gen; // seed the generator
    std::uniform_real_distribution<> distr;

    float timer                = 0.0f;
    float directionalNearPlane = 1.0f;
    float directionalFarPlane  = 100.0f;
    float pointNearPlane       = 0.1f;
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

    LightObject                 directionalLight;
    std::vector<LightObject>    torchLights;
    LightObject                 redLight;
    std::vector<TorchFireGroup> torchGroups;

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

    // Models
    StaticMeshObject sponza;
    Ref<Model>       sponzaModel;

    StaticMeshObject helmet;
    Ref<Model>       helmetModel;

    StaticMeshObject sword;
    Ref<Model>       swordModel;

    StaticMeshObject torch;
    StaticMeshObject torch2;
    StaticMeshObject torch3;
    StaticMeshObject torch4;
    Ref<Model>       torchModel;

    StaticMeshObject skybox;
    Ref<Model>       skyboxModel;

    StaticMeshObject cube;
    Ref<Model>       cubeModel;

    Ref<ParticleSystem> ambientParticles;
    float               lightFlickerRate      = 0.07f;
    float               aniamtionRate         = 0.013888888f;
    int                 currentAnimationFrame = 0;

    GlobalParametersUBO globalParametersUBO;
    VkBuffer            globalParametersUBOBuffer;
    VkDeviceMemory      globalParametersUBOBufferMemory;
    void*               mappedGlobalParametersModelUBOBuffer;

    Ref<Material> pbrMaterial;
    Ref<Material> emissiveMaterial;
    Ref<Material> skyboxMaterial;
    Ref<Material> cubeMaterial;

    // Others
    VkCommandBuffer cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool   cmdPool;
    Ref<Bloom>      bloomAgent;
    VkSampler       finalPassSampler;
    VkDescriptorSet finalPassDescriptorSet;

    glm::mat4 directionalLightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, directionalNearPlane, directionalFarPlane);
    glm::mat4 pointLightProjectionMatrix       = glm::perspective(glm::radians(90.0f), 1.0f, pointNearPlane, pointFarPlane);

   private:
    // Framebuffer creations.
    void CreateHDRFramebuffer();
    void CreateSwapchainFramebuffers();
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
    void CreateSwapchainRenderPass();
    void CreateBokehRenderPass();
    void CreateHDRRenderPass();
    void CreateShadowRenderPass();
    void CreatePointShadowRenderPass();

    // Specific funcs.
    void SetupParticleSystems();
    void EnableDepthOfField();
    void DisableDepthOfField();
    void SyncLightsToUBO();
    void SetupTorchesAndLights();

   public:
    ForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera);

    // RendererInterface overrides
    void Init() override;
    bool BeginFrame() override;
    void RenderFrame(const float InDeltaTime) override;
    void EndFrame() override;
    void Cleanup() override;
    // ~RendererInterface overrides

    void UpdateViewport_Scissor();
    void InitImGui();
    void RenderImGui();
    void HandleWindowResize(VkResult InResult);

   private:
    VulkanContext& _Context;
    Ref<Swapchain> _Swapchain;
    Ref<Camera>    _Camera;

    Unique<RenderPass> _PointShadowRenderPass;
    Unique<RenderPass> _HDRRenderPass;
    Unique<RenderPass> _ShadowMapRenderPass;
    Unique<RenderPass> _SwapchainRenderPass;

    Ref<Framebuffer>              _DirectionalShadowMapFramebuffer;
    Ref<Framebuffer>              _HDRFramebuffer;
    std::vector<Ref<Framebuffer>> _PointShadowMapFramebuffers;
    std::vector<Ref<Framebuffer>> _SwapchainFramebuffers;

    VkViewport _DynamicViewport{};
    VkRect2D   _DynamicScissor;

    uint32_t _CurrentSwapchainImageIndex = 0;

    Ref<CloudPass> _CloudPass;

    // Descriptor Pools
    Ref<DescriptorPool>                   pool;
    std::chrono::steady_clock::time_point startTime;

    Unique<RenderGraph> _Graph;

    int _FrameCount = 0;

    float _DeltaTime;
};
