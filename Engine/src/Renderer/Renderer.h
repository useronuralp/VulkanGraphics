#pragma once
// #include "OVKLib.h"
#include "Pipeline.h"
#include "Renderer/RenderPass.h"

// TODO: Move somewhere else
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <random>

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

#define MAX_FRAMES_IN_FLIGHT  3
#define MAX_POINT_LIGHT_COUNT 10
#define SHADOW_DIM            10000
#define POUNT_SHADOW_DIM      1000

class RendererInterface
{
   public:
    virtual ~RendererInterface()              = default;

    virtual void Init()                       = 0; // Initialize renderer resources
    virtual bool BeginFrame()                 = 0; // Start command buffer/frame
    virtual void RenderFrame(float DeltaTime) = 0; // Render the main scene
    virtual void EndFrame()                   = 0; // Submit frame
    virtual void InitImGui()                  = 0; // Submit frame
    virtual void PollEvents()                 = 0; // Submit frame
    virtual void RenderImGui()                = 0; // Submit frame
    virtual void Cleanup()                    = 0; // Submit frame
};

class ForwardRenderer : public RendererInterface
{
   public:
    VkDescriptorPool          imguiPool;
    ImGui_ImplVulkan_InitInfo init_info;

    bool pointLightShadows  = true;
    bool showDOFFocus       = false;
    bool enableDepthOfField = true;

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

    // Attachments. Each framebuffer can have multiple attachments.
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

    // Descriptor Pools
    Ref<DescriptorPool> pool;

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
    Ref<Model> model;
    Ref<Model> model2;
    Ref<Model> model3;
    Ref<Model> torch;
    Ref<Model> skybox;
    Ref<Model> cube;

    Ref<ParticleSystem> fireBase;
    Ref<ParticleSystem> fireBase2;
    Ref<ParticleSystem> fireBase3;
    Ref<ParticleSystem> fireBase4;
    Ref<ParticleSystem> fireSparks;
    Ref<ParticleSystem> fireSparks2;
    Ref<ParticleSystem> fireSparks3;
    Ref<ParticleSystem> fireSparks4;
    Ref<ParticleSystem> ambientParticles;

    GlobalParametersUBO globalParametersUBO;
    VkBuffer            globalParametersUBOBuffer;
    VkDeviceMemory      globalParametersUBOBufferMemory;
    void*               mappedGlobalParametersModelUBOBuffer;

    // Others
    VkCommandBuffer cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool   cmdPool;
    Ref<Bloom>      bloomAgent;
    VkSampler       finalPassSampler;
    VkDescriptorSet finalPassDescriptorSet;

    std::random_device               rd; // obtain a random number from hardware
    std::mt19937                     gen; // seed the generator
    std::uniform_real_distribution<> distr;

    VkVertexInputBindingDescription                bindingDescription{};
    VkVertexInputBindingDescription                bindingDescription2{};
    VkVertexInputBindingDescription                bindingDescription3{};
    VkVertexInputBindingDescription                bindingDescription4{};
    VkVertexInputBindingDescription                bindingDescription5{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions2{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions3{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions4{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions5{};

    glm::mat4 torch1modelMatrix{ 1.0 };
    glm::mat4 torch2modelMatrix{ 1.0 };
    glm::mat4 torch3modelMatrix{ 1.0 };
    glm::mat4 torch4modelMatrix{ 1.0 };

    float     lightFlickerRate         = 0.07f;
    float     aniamtionRate            = 0.013888888f;
    int       currentAnimationFrame    = 0;
    float     timer                    = 0.0f;
    glm::vec4 directionalLightPosition = glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f);
    float     directionalNearPlane     = 1.0f;
    float     directionalFarPlane      = 100.0f;
    float     pointNearPlane           = 0.1f;
    float     pointFarPlane            = 100.0f;
    int       frameCount               = 0;

    glm::mat4 directionalLightProjectionMatrix =
        glm::perspective(glm::radians(45.0f), 1.0f, directionalNearPlane, directionalFarPlane);
    glm::mat4 pointLightProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, pointNearPlane, pointFarPlane);

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

   private:
    void CreateHDRFramebuffer();
    void CreateSwapchainFramebuffers();
    void CreateBokehFramebuffer();

    void SetupPBRPipeline();
    void SetupFinalPassPipeline();
    void SetupShadowPassPipeline();
    void SetupPointShadowPassPipeline();
    void SetupBokehPassPipeline();
    void SetupSkyboxPipeline();
    void SetupCubePipeline();
    void SetupParticleSystemPipeline();
    void SetupEmissiveObjectPipeline();

    void CreateSwapchainRenderPass();
    void CreateBokehRenderPass();
    void CreateHDRRenderPass();
    void CreateShadowRenderPass();
    void CreatePointShadowRenderPass();

    void SetupParticleSystems();
    void EnableDepthOfField();
    void DisableDepthOfField();

   public:
    ForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera);

    void Init();
    bool BeginFrame();
    void RenderFrame(const float InDeltaTime);
    void EndFrame();
    void Cleanup();
    void UpdateViewport_Scissor();

    void InitImGui();
    void CreateSynchronizationPrimitives();
    void PollEvents();
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

    uint32_t                 _ConcurrentAllowedFrameCount = MAX_FRAMES_IN_FLIGHT;
    uint32_t                 _CurrentBufferIndex          = 0;
    std::vector<VkSemaphore> _RenderingCompleteSemaphores;
    std::vector<VkSemaphore> _AcquireFinishedSemaphores;
    std::vector<VkFence>     _InFlightFences;

    uint32_t _CurrentSwapchainImageIndex = 0;

    float _DeltaTime;
};
