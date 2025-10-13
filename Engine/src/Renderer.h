#pragma once
#include "OVKLib.h"

class VulkanContext;
class Swapchain;
class Pipeline;
class Framebuffer;
class Camera;
class Image;
class Model;
class ParticleSystem;
class DescriptorSetLayout;

#define MAX_FRAMES_IN_FLIGHT  1
#define MAX_POINT_LIGHT_COUNT 10
#define SHADOW_DIM            10000
#define POUNT_SHADOW_DIM      1000
#define MAX_FRAMES_IN_FLIGHT  1
#define READY_TO_ACQUIRE      -1
#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

class Renderer {
   public:
    // Vulkan Application members
    float m_Time = 0.0f;

    VkSampleCountFlagBits m_MSAA;

    uint32_t                 m_FramesInFlight = MAX_FRAMES_IN_FLIGHT;
    uint32_t                 m_CurrentFrame   = 0;
    std::vector<VkSemaphore> m_RenderingCompleteSemaphores;
    std::vector<VkSemaphore> m_ImageAvailableSemaphores;
    std::vector<VkFence>     m_InFlightFences;
    // ~Vulkan Application members

    uint32_t m_ActiveImageIndex = -1;

    // std::shared_ptr<Camera>    s_Camera;
    // std::shared_ptr<Swapchain> s_Swapchain;

    VkDescriptorPool          imguiPool;
    ImGui_ImplVulkan_InitInfo init_info;

    bool pointLightShadows  = true;
    bool showDOFFocus       = false;
    bool enableDepthOfField = true;

    struct GlobalParametersUBO {
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

    struct CloudParametersUBO {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::vec4 BoundsMax;
        glm::vec4 BoundsMin;
        glm::vec4 CameraPosition;
    };

    VkClearValue                depthPassClearValue;
    std::array<VkClearValue, 2> clearValues;

    Ref<Framebuffer>              directionalShadowMapFramebuffer;
    Ref<Framebuffer>              HDRFramebuffer;
    std::vector<Ref<Framebuffer>> pointShadowMapFramebuffers;
    // Attachments. Each framebuffer can have multiple attachments.
    Ref<Image>              directionalShadowMapImage;
    Ref<Image>              HDRColorImage;
    Ref<Image>              HDRDepthImage;
    Ref<Image>              particleTexture;
    Ref<Image>              worleyNoiseTexture;
    Ref<Image>              dustTexture;
    Ref<Image>              fireTexture;
    std::vector<Ref<Image>> pointShadowMaps;

    // Render pass begin infos.
    VkRenderPassBeginInfo depthPassBeginInfo;
    VkRenderPassBeginInfo pointShadowPassBeginInfo;
    VkRenderPassBeginInfo finalScenePassBeginInfo;
    VkRenderPassBeginInfo HDRRenderPassBeginInfo;

    // Render passes
    VkRenderPass shadowMapRenderPass;
    VkRenderPass pointShadowRenderPass;
    VkRenderPass HDRRenderPass;

    // Descriptor Set Layouts
    Ref<DescriptorSetLayout> swapchainLayout;
    Ref<DescriptorSetLayout> emissiveLayout;
    Ref<DescriptorSetLayout> PBRLayout;
    Ref<DescriptorSetLayout> skyboxLayout;
    Ref<DescriptorSetLayout> cubeLayout;
    Ref<DescriptorSetLayout> cloudLayout;
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
    Ref<Pipeline> cloudPipeline;
    Ref<Pipeline> particleSystemPipeline;

    // Models
    Model* model;
    Model* model2;
    Model* model3;
    Model* torch;
    Model* skybox;
    Model* cube;
    Model* clouds;

    ParticleSystem* fireSparks;
    ParticleSystem* fireBase;

    ParticleSystem* fireSparks2;
    ParticleSystem* fireBase2;

    ParticleSystem* fireSparks3;
    ParticleSystem* fireBase3;

    ParticleSystem* fireSparks4;
    ParticleSystem* fireBase4;

    ParticleSystem* ambientParticles;

    GlobalParametersUBO globalParametersUBO;
    VkBuffer            globalParametersUBOBuffer;
    VkDeviceMemory      globalParametersUBOBufferMemory;
    void*               mappedGlobalParametersModelUBOBuffer;

    CloudParametersUBO cloudParametersUBO;
    VkBuffer           cloudParametersUBOBuffer;
    VkDeviceMemory     cloudParametersUBOBufferMemory;
    void*              mappedCloudParametersUBOBuffer;

    // Others
    VkCommandBuffer cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool   cmdPool;
    Ref<Bloom>      bloomAgent;
    VkSampler       finalPassSampler;
    VkSampler       finalPassWorleySampler;
    VkDescriptorSet finalPassDescriptorSet;

    std::random_device               rd; // obtain a random number from hardware
    std::mt19937                     gen; // seed the generator
    std::uniform_real_distribution<> distr;

    VkVertexInputBindingDescription                bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    VkVertexInputBindingDescription                bindingDescription2{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions2{};

    VkVertexInputBindingDescription                bindingDescription3{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions3{};

    VkVertexInputBindingDescription                bindingDescription4{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions4{};

    VkVertexInputBindingDescription                bindingDescription5{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions5{};

    glm::mat4 torch1modelMatrix{ 1.0 };
    glm::mat4 torch2modelMatrix{ 1.0 };
    glm::mat4 torch3modelMatrix{ 1.0 };
    glm::mat4 torch4modelMatrix{ 1.0 };

    float lightFlickerRate             = 0.07f;
    float aniamtionRate                = 0.013888888f;
    int   currentAnimationFrame        = 0;
    float timer                        = 0.0f;

    glm::vec4 directionalLightPosition = glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f);

    float directionalNearPlane         = 1.0f;
    float directionalFarPlane          = 100.0f;

    float pointNearPlane               = 0.1f;
    float pointFarPlane                = 100.0f;

    int frameCount                     = 0;

    glm::mat4 directionalLightProjectionMatrix =
        glm::perspective(glm::radians(45.0f), 1.0f, directionalNearPlane, directionalFarPlane);
    glm::mat4 pointLightProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, pointNearPlane, pointFarPlane);

    float    GetRenderTime();
    uint32_t CurrentFrameIndex();

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame = GetRenderTime();

    // Experimental
    Ref<Image>            bokehPassImage;
    VkRenderPassBeginInfo bokehPassBeginInfo;
    VkRenderPass          bokehRenderPass;
    Ref<Framebuffer>      bokehPassFramebuffer;
    Ref<Pipeline>         bokehPassPipeline;

    VkRenderPassBeginInfo    bokehRenderPassBeginInfo;
    VkSampler                bokehPassSceneSampler;
    VkSampler                bokehPassDepthSampler;
    VkDescriptorSet          bokehDescriptorSet;
    Ref<DescriptorSetLayout> bokehPassLayout;

   private:
    void  SetupBokehPassPipeline();
    void  CreateBokehRenderPass();
    float DeltaTime();
    void  CreateHDRFramebuffer();
    void  CreateBokehFramebuffer();
    void  SetupPBRPipeline();
    void  SetupFinalPassPipeline();
    void  SetupShadowPassPipeline();
    void  SetupPointShadowPassPipeline();
    void  SetupSkyboxPipeline();
    void  SetupCubePipeline();

    void SetupCloudPipeline();
    void SetupParticleSystemPipeline();
    void SetupEmissiveObjectPipeline();
    void SetupParticleSystems();
    void CreateHDRRenderPass();
    void CreateShadowRenderPass();
    void CreatePointShadowRenderPass();
    void EnableDepthOfField();
    void DisableDepthOfField();

   public:
    Renderer(VulkanContext& InContext, std::shared_ptr<Swapchain> InSwapchain, std::shared_ptr<Camera> InCamera);

    // static Renderer* Get();

    void Init();
    void BeginFrame();
    void RenderScene();
    void EndFrame();
    void RecreateSwapchain();

    void WindowResize();
    void InitImGui();

    void Update();
    void RunGlobal();

    void Cleanup();

   private:
    void CreateSynchronizationPrimitives();

    void CreateRenderPasses();
    void CreateFramebuffers();
    void CreatePipelines();
    void CreateDescriptorLayouts();
    void CreateSyncObjects();

   private:
    VulkanContext&             _Context;
    std::shared_ptr<Swapchain> _Swapchain;
    std::shared_ptr<Camera>    _Camera;

    // Vulkan resources
    VkRenderPass _HDRRenderPass;
    VkRenderPass _ShadowRenderPass;
    VkRenderPass _PointShadowRenderPass;

    Ref<Framebuffer>              _HDRFramebuffer;
    Ref<Framebuffer>              _DirectionalShadowFramebuffer;
    std::vector<Ref<Framebuffer>> _PointShadowFramebuffers;

    std::vector<VkSemaphore> _ImageAvailableSemaphores;
    std::vector<VkSemaphore> _RenderFinishedSemaphores;
    std::vector<VkFence>     _InFlightFences;

    uint32_t _CurrentFrame = 0;

    // static Renderer* _Instance;
};
