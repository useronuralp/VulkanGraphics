#include "Bloom.h"
#include "Camera.h"
#include "CloudPass.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Instance.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshBindingHelper.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Renderer/RenderPass.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"
#include "VulkanContext.h"
#include "Window.h"

#include <Curl.h>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <iostream>

static bool printedDependencies = false;

extern FrameSync GFrameSync;

ForwardRenderer::ForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
    : _Context(InContext), _Swapchain(InSwapchain), _Camera(InCamera)
{
}

void ForwardRenderer::Init()
{
    _Graph    = make_u<RenderGraph>(_Context);
    startTime = std::chrono::high_resolution_clock::now();

    UpdateViewport_Scissor();

    // Create the pool(s) that we need here.
    pool = make_s<DescriptorPool>(
        200,
        std::vector<VkDescriptorType>{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });

    auto extent = _Context.GetSurface()->GetVKExtent();
    auto device = _Context.GetDevice()->GetVKDevice();
    _CloudPass  = make_s<CloudPass>();

    _CloudPass->Create(device, extent, pool->GetDescriptorPool());

    globalParametersUBO.pointLightCount = glm::vec4(5);

    CurlNoise::SetCurlSettings(false, 4.0f, 6, 1.0, 0.0);
    pointShadowMaps.resize(globalParametersUBO.pointLightCount.x);
    _PointShadowMapFramebuffers.resize(globalParametersUBO.pointLightCount.x);

    std::vector<DescriptorBinding> hdrLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // Diffuse
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // Normal
        { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // Roughness/Metallic
        { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT }, // Shadow map
        { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, VK_SHADER_STAGE_FRAGMENT_BIT }, // Point shadows
    };

    std::vector<DescriptorBinding> SkyboxLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<DescriptorBinding> ParticleSystemLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<DescriptorBinding> SwapchainLayout{
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    std::vector<DescriptorBinding> EmissiveLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
    };

    std::vector<DescriptorBinding> CubeLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT },
    };

    std::vector<DescriptorBinding> BokehPassLayout{
        { 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
    };

    // Descriptor Set Layouts
    particleSystemLayout = make_s<DescriptorSetLayout>(ParticleSystemLayout);
    skyboxLayout         = make_s<DescriptorSetLayout>(SkyboxLayout);
    cubeLayout           = make_s<DescriptorSetLayout>(CubeLayout);
    PBRLayout            = make_s<DescriptorSetLayout>(hdrLayout);
    swapchainLayout      = make_s<DescriptorSetLayout>(SwapchainLayout);
    emissiveLayout       = make_s<DescriptorSetLayout>(EmissiveLayout);
    bokehPassLayout      = make_s<DescriptorSetLayout>(BokehPassLayout);

    // Following are the global Uniform Buffes shared by all shaders.
    Utils::CreateVKBuffer(
        sizeof(GlobalParametersUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        globalParametersUBOBuffer,
        globalParametersUBOBufferMemory);
    vkMapMemory(_Context.GetDevice()->GetVKDevice(), globalParametersUBOBufferMemory, 0, sizeof(GlobalParametersUBO), 0, &mappedGlobalParametersModelUBOBuffer);

    // Create an image for the shadowmap. We will render to this image when
    // we are doing a shadow pass.
    directionalShadowMapImage =
        make_s<Image>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), ImageType::DEPTH);

    for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
    {
        pointShadowMaps[i] = make_s<Image>(
            POUNT_SHADOW_DIM,
            POUNT_SHADOW_DIM,
            VK_FORMAT_D32_SFLOAT,
            (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
            ImageType::DEPTH_CUBEMAP);
    }

    // Allocate final pass descriptor Set.
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &swapchainLayout->GetDescriptorLayout();

    VkResult rslt                = vkAllocateDescriptorSets(_Context.GetDevice()->GetVKDevice(), &allocInfo, &finalPassDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    // Allocate bokeh pass descriptor Set.
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &bokehPassLayout->GetDescriptorLayout();

    rslt                         = vkAllocateDescriptorSets(_Context.GetDevice()->GetVKDevice(), &allocInfo, &bokehDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    // Setup resources.
    CreateSwapchainRenderPass();
    CreateHDRRenderPass();
    CreateShadowRenderPass();
    CreatePointShadowRenderPass();
    CreateBokehRenderPass();

    CreateSwapchainFramebuffers();
    CreateHDRFramebuffer();
    CreateBokehFramebuffer();

    SetupFinalPassPipeline();
    SetupPBRPipeline();
    SetupShadowPassPipeline();
    SetupSkyboxPipeline();
    SetupCubePipeline();
    SetupPointShadowPassPipeline();
    SetupEmissiveObjectPipeline();
    SetupParticleSystemPipeline();
    SetupBokehPassPipeline();

    // Materials.
    pbrMaterial      = make_s<Material>("PBR", pipeline, PBRLayout);
    emissiveMaterial = make_s<Material>("Emissive", EmissiveObjectPipeline, emissiveLayout);
    skyboxMaterial   = make_s<Material>("Skybox", skyboxPipeline, skyboxLayout);
    cubeMaterial     = make_s<Material>("Cube", cubePipeline, cubeLayout);

    // Directional light shadowmap framebuffer.
    std::vector<VkImageView> attachments = { directionalShadowMapImage->GetImageView() };

    _DirectionalShadowMapFramebuffer     = make_s<Framebuffer>(_ShadowMapRenderPass->GetHandle(), attachments, SHADOW_DIM, SHADOW_DIM);

    // Framebuffers need for point light shadows. (Dependent on the number
    // of point lights in the scene)
    for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
    {
        attachments                    = { pointShadowMaps[i]->GetImageView() };

        _PointShadowMapFramebuffers[i] = make_s<Framebuffer>(_PointShadowRenderPass->GetHandle(), attachments, POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, 6);
    }

    // Loading the model Sponza
    sponzaModel = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/Sponza/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);
    MeshBinding::BindModelPBR(sponzaModel, pool, PBRLayout, globalParametersUBOBuffer, sizeof(GlobalParametersUBO), directionalShadowMapImage, pointShadowMaps);

    sponza = StaticMeshObject("Sponza", sponzaModel);
    sponza.SetScale(glm::vec3(0.005f, 0.005f, 0.005f));
    sponza.SetMaterial(pbrMaterial);

    // Loading the model Malenia's Helmet.
    helmetModel = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/MaleniaHelmet/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);
    MeshBinding::BindModelPBR(helmetModel, pool, PBRLayout, globalParametersUBOBuffer, sizeof(GlobalParametersUBO), directionalShadowMapImage, pointShadowMaps);

    helmet = StaticMeshObject("MaleniaHelmet", helmetModel);
    helmet.SetPosition(glm::vec3(0.0, 2.0, 0.0));
    helmet.Rotate(90, glm::vec3(0, 1, 0));
    helmet.SetScale(glm::vec3(0.7, 0.7, 0.7));
    helmet.SetMaterial(pbrMaterial);

    SetupTorchesAndLights();
    SetupParticleSystems();

    directionalLight = LightObject("Directional Light", LightType::Directional);
    directionalLight.SetPosition(glm::vec3(glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f)));
    directionalLight.SetIntensity(10.0f);
    directionalLight.SetColor(glm::vec3(1.0f));
    directionalLight.SetCastsShadow(true);

    globalParametersUBO.cameraNearPlane = glm::vec4(_Camera->GetNearClip());
    globalParametersUBO.cameraFarPlane  = glm::vec4(_Camera->GetFarClip());
    globalParametersUBO.focalDepth      = glm::vec4(1.5f);
    globalParametersUBO.focalLength     = glm::vec4(15.0f);
    globalParametersUBO.fstop           = glm::vec4(6.0f);
    globalParametersUBO.pointFarPlane   = glm::vec4(pointFarPlane);

    swordModel                          = make_s<Model>(Utils::NormalizePath(std::string(SOLUTION_DIR) + "Engine/assets/models/sword/scene.gltf"), LOAD_VERTEX_POSITIONS);
    MeshBinding::BindModelSimple(swordModel, pool, emissiveLayout, globalParametersUBOBuffer, sizeof(glm::mat4) * 2);

    sword = StaticMeshObject("Torch", swordModel);
    sword.SetPosition(glm::vec3(-2, 7, 0));
    sword.Rotate(54, glm::vec3(0, 0, 1));
    sword.Rotate(90, glm::vec3(0, 1, 0));
    sword.SetScale(glm::vec3(0.7, 0.7, 0.7));
    sword.SetMaterial(emissiveMaterial);

    // Vertex data for the skybox.
    const uint32_t vertexCount               = 3 * 6 * 6;
    const float    cubeVertices[vertexCount] = {
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

        1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
    };

    // Loading the necessary images for the skybox one by one.
    std::string front  = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/front.png");
    std::string back   = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/back.png");
    std::string top    = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/top.png");
    std::string bottom = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/bottom.png");
    std::string right  = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/right.png");
    std::string left   = (std::string(SOLUTION_DIR) + "Engine/assets/textures/skybox/Night/left.png");

    // Set up the 6 sided texture for the skybox by using the above images.
    std::vector<std::string> skyboxTex{ right, left, top, bottom, front, back };
    Ref<Image>               cubemap = make_s<Image>(skyboxTex, VK_FORMAT_R8G8B8A8_SRGB);

    // Create the mesh for the skybox.
    skyboxModel = make_s<Model>(cubeVertices, vertexCount, cubemap);
    MeshBinding::BindSkybox(skyboxModel->GetMeshes()[0], pool, skyboxLayout, globalParametersUBOBuffer);

    skybox = StaticMeshObject("Skybox", skyboxModel);
    skybox.SetMaterial(skyboxMaterial);

    // A cube model to depict/debug point lights.
    cubeModel = make_s<Model>(cubeVertices, vertexCount);
    MeshBinding::BindSimple(cubeModel->GetMeshes()[0], pool, cubeLayout, globalParametersUBOBuffer, sizeof(glm::mat4) * 2);

    cube = StaticMeshObject("Cube", cubeModel);
    cube.SetPosition(glm::vec3(-0.3f, 3.190f, -0.180f));
    cube.SetScale(glm::vec3(0.05f));
    cube.SetMaterial(cubeMaterial);

    redLight = LightObject("Red Light", LightType::Point);
    redLight.SetPosition(cube.GetPosition());
    redLight.SetColor(glm::vec3(1.0f, 0.0f, 0.0f));
    redLight.SetIntensity(500.0f);
    redLight.SetCastsShadow(true);

    CommandBuffer::CreateCommandBufferPool(_Context._QueueFamilies.GraphicsFamily, cmdPool);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CommandBuffer::CreateCommandBuffer(cmdBuffers[i], cmdPool);
    }

    bloomAgent = make_s<Bloom>();
    bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage, _CloudPass);

    finalPassSampler = Utils::CreateSampler(bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);

    Utils::UpdateDescriptorSet(finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassSceneSampler =
        Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        bokehDescriptorSet, bokehPassSceneSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassDepthSampler = Utils::CreateSampler(HDRDepthImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(bokehDescriptorSet, bokehPassDepthSampler, HDRDepthImage->GetImageView(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    Utils::UpdateDescriptorSet(bokehDescriptorSet, globalParametersUBOBuffer, offsetof(GlobalParametersUBO, DOFFramebufferSize), sizeof(glm::vec4) * 7, 2);
}

void ForwardRenderer::SetupTorchesAndLights()
{
    torchModel = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/torch/scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);
    MeshBinding::BindModelPBR(torchModel, pool, PBRLayout, globalParametersUBOBuffer, sizeof(GlobalParametersUBO), directionalShadowMapImage, pointShadowMaps);

    torch  = StaticMeshObject("Torch", torchModel);
    torch2 = StaticMeshObject("Torch2", torchModel);
    torch3 = StaticMeshObject("Torch3", torchModel);
    torch4 = StaticMeshObject("Torch4", torchModel);

    torch.SetPosition(glm::vec3(2.450, 1.3, 0.810));
    torch.Rotate(90.0, glm::vec3(0, 1, 0));
    torch.SetScale(glm::vec3(0.3f, 0.3f, 0.3f));
    torch.SetMaterial(pbrMaterial);

    torch2.SetPosition(glm::vec3(0.610, 1.3, -1.170));
    torch2.Rotate(-90.0, glm::vec3(0, 1, 0));
    torch2.SetScale(glm::vec3(0.3f, 0.3f, 0.3f));
    torch2.SetMaterial(pbrMaterial);

    torch3.SetPosition(glm::vec3(0.610, 1.3, 0.81));
    torch3.Rotate(90.0, glm::vec3(0, 1, 0));
    torch3.SetScale(glm::vec3(0.3f, 0.3f, 0.3f));
    torch3.SetMaterial(pbrMaterial);

    torch4.SetPosition(glm::vec3(2.45, 1.3, -1.170));
    torch4.Rotate(-90.0, glm::vec3(0, 1, 0));
    torch4.SetScale(glm::vec3(0.3f, 0.3f, 0.3f));
    torch4.SetMaterial(pbrMaterial);

    std::vector<StaticMeshObject*> torches = {
        { &torch },
        { &torch2 },
        { &torch3 },
        { &torch4 },
    };

    for (int i = 0; i < torches.size(); i++)
    {
        auto& torch = *torches[i];

        LightObject light("Torch Light " + std::to_string(i), LightType::Point);
        light.SetPosition(glm::vec3(torch.GetPosition().x, torch.GetPosition().y + 0.22f, torch.GetPosition().z));
        light.SetColor(glm::vec3(0.97f, 0.76f, 0.46f));
        light.SetIntensity(25.0f);
        light.SetCastsShadow(true);

        torchLights.push_back(std::move(light));
    }
}

void ForwardRenderer::SetupPBRPipeline()
{
    pipeline = PipelineBuilder(_Context)
                   .SetRenderPass(_HDRRenderPass->GetHandle())
                   .SetDescriptorSetLayout(PBRLayout)
                   .SetVertexShader("assets/shaders/PBRShaderVERT.spv")
                   .SetFragmentShader("assets/shaders/PBRShaderFRAG.spv")
                   .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                   .SetVertexBindings({ { 0, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) * 3, VK_VERTEX_INPUT_RATE_VERTEX } })
                   .SetVertexAttributes(
                       {
                           { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
                           { 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(glm::vec3) },
                           { 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) },
                           { 3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) },
                           { 4, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) * 2 },
                       })
                   .Build();
}

void ForwardRenderer::SetupFinalPassPipeline()
{
    finalPassPipeline = PipelineBuilder(_Context)
                            .SetRenderPass(_SwapchainRenderPass->GetHandle())
                            .SetDescriptorSetLayout(swapchainLayout)
                            .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                            .SetFragmentShader("assets/shaders/swapchainFRAG.spv")
                            .Build();
}
void ForwardRenderer::SetupShadowPassPipeline()
{
    shadowPassPipeline = PipelineBuilder(_Context)
                             .SetRenderPass(_ShadowMapRenderPass->GetHandle())
                             .SetDescriptorSetLayout(PBRLayout)
                             .SetVertexShader("assets/shaders/shadowPassVERT.spv")
                             .SetDepthBias(1.25f, 0.0f, 1.75f)
                             .SetBlending(VK_FALSE)
                             .SetFixedViewport(SHADOW_DIM, SHADOW_DIM)
                             .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                             .SetVertexBindings({ { 0, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) * 3, VK_VERTEX_INPUT_RATE_VERTEX } })
                             .SetVertexAttributes({ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } })
                             .SetDynamicStatesEnabled(false)
                             .Build();
}
void ForwardRenderer::SetupPointShadowPassPipeline()
{
    pointShadowPassPipeline = PipelineBuilder(_Context)
                                  .SetRenderPass(_PointShadowRenderPass->GetHandle())
                                  .SetDescriptorSetLayout(PBRLayout)
                                  .SetVertexShader("assets/shaders/pointShadowPassVERT.spv")
                                  .SetFragmentShader("assets/shaders/pointShadowPassFRAG.spv")
                                  .SetGeometryShader("assets/shaders/pointShadowPassGEOM.spv")
                                  .SetBlending(VK_FALSE)
                                  .SetFixedViewport(POUNT_SHADOW_DIM, POUNT_SHADOW_DIM)
                                  .SetDepthBias(1.25, 0.0, 1.75)
                                  .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                                  .AddPushConstant(VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4))
                                  .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec4) * 2)
                                  .SetVertexBindings({ { 0, sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) * 3, VK_VERTEX_INPUT_RATE_VERTEX } })
                                  .SetVertexAttributes({ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } })
                                  .SetDynamicStatesEnabled(false)
                                  .Build();
}
void ForwardRenderer::SetupSkyboxPipeline()
{
    skyboxPipeline = PipelineBuilder(_Context)
                         .SetRenderPass(_HDRRenderPass->GetHandle())
                         .SetDescriptorSetLayout(skyboxLayout)
                         .SetVertexShader("assets/shaders/cubemapVERT.spv")
                         .SetFragmentShader("assets/shaders/cubemapFRAG.spv")
                         .SetDepthTest(VK_FALSE)
                         .SetDepthWrite(VK_FALSE)
                         .SetBlending(VK_FALSE)
                         .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4))
                         .SetVertexBindings({ { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX } })
                         .SetVertexAttributes({ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } })
                         .Build();
}
void ForwardRenderer::SetupCubePipeline()
{
    cubePipeline = PipelineBuilder(_Context)
                       .SetRenderPass(_HDRRenderPass->GetHandle())
                       .SetDescriptorSetLayout(cubeLayout)
                       .SetVertexShader("assets/shaders/emissiveShaderVERT.spv")
                       .SetFragmentShader("assets/shaders/emissiveShaderFRAG.spv")
                       .SetBlending(VK_FALSE)
                       .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4) * 2)
                       .SetVertexBindings({ { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX } })
                       .SetVertexAttributes({ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } })
                       .Build();
}

void ForwardRenderer::SetupParticleSystemPipeline()
{
    particleSystemPipeline = PipelineBuilder(_Context)
                                 .SetRenderPass(_HDRRenderPass->GetHandle())
                                 .SetDescriptorSetLayout(particleSystemLayout)
                                 .SetVertexShader("assets/shaders/particleVERT.spv")
                                 .SetFragmentShader("assets/shaders/particleFRAG.spv")
                                 .SetDepthWrite(VK_FALSE)
                                 .SetTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
                                 .SetAlphaBlendFactors(VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ZERO)
                                 .AddPushConstant(VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4))
                                 .SetVertexBindings({ { 0, sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX } })
                                 .SetVertexAttributes(
                                     {
                                         { 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, Position) },
                                         { 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Particle, Color) },
                                         { 2, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, Alpha) },
                                         { 3, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, SizeRadius) },
                                         { 4, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, Rotation) },
                                         { 5, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, RowOffset) },
                                         { 6, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, ColumnOffset) },
                                         { 7, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, RowCellSize) },
                                         { 8, 0, VK_FORMAT_R32_SFLOAT, offsetof(Particle, ColumnCellSize) },
                                     })
                                 .Build();
}
void ForwardRenderer::SetupEmissiveObjectPipeline()
{
    EmissiveObjectPipeline = PipelineBuilder(_Context)
                                 .SetRenderPass(_HDRRenderPass->GetHandle())
                                 .SetDescriptorSetLayout(emissiveLayout)
                                 .SetVertexShader("assets/shaders/emissiveShaderVERT.spv")
                                 .SetFragmentShader("assets/shaders/emissiveShaderFRAG.spv")
                                 .AddPushConstant(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4))
                                 .SetVertexBindings({ { 0, sizeof(glm::vec3), VK_VERTEX_INPUT_RATE_VERTEX } })
                                 .SetVertexAttributes({ { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 } })
                                 .Build();
}

void ForwardRenderer::SetupParticleSystems()
{
    particleTexture = make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/spark.png") }, VK_FORMAT_R8G8B8A8_SRGB);
    fireTexture     = make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/fire_sprite_sheet.png") }, VK_FORMAT_R8G8B8A8_SRGB);
    dustTexture     = make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/dust.png") }, VK_FORMAT_R8G8B8A8_SRGB);

    std::vector<StaticMeshObject*> torches = { &torch, &torch2, &torch3, &torch4 };
    std::vector<LightObject*>      lights  = { &torchLights[0], &torchLights[1], &torchLights[2], &torchLights[3] };

    for (int i = 0; i < torches.size(); i++)
    {
        glm::vec3 pos = torches[i]->GetPosition();

        ParticleSpecs sparkSpecs{};
        sparkSpecs.ParticleCount       = 10;
        sparkSpecs.EnableNoise         = true;
        sparkSpecs.TrailLength         = 2;
        sparkSpecs.SphereRadius        = 0.05f;
        sparkSpecs.ImmortalParticle    = false;
        sparkSpecs.ParticleSize        = 0.5f;
        sparkSpecs.EmitterPos          = pos + glm::vec3(0.0f, 0.22f, 0.0f);
        sparkSpecs.ParticleMinLifetime = 0.1f;
        sparkSpecs.ParticleMaxLifetime = 3.0f;
        sparkSpecs.MinVel              = glm::vec3(-1.0f, 0.1f, -1.0f);
        sparkSpecs.MaxVel              = glm::vec3(1.0f, 2.0f, 1.0f);

        auto sparks                    = make_s<ParticleSystem>(sparkSpecs, particleTexture, particleSystemLayout, pool);
        sparks->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

        ParticleSpecs flameSpecs{};
        flameSpecs.ParticleCount       = 1;
        flameSpecs.ImmortalParticle    = true;
        flameSpecs.ParticleSize        = 13.0f;
        flameSpecs.EnableNoise         = false;
        flameSpecs.SphereRadius        = 0.0f;
        flameSpecs.TrailLength         = 0;
        flameSpecs.EmitterPos          = pos + glm::vec3(0.0f, 0.28f, 0.0f);
        flameSpecs.ParticleMinLifetime = 0.1f;
        flameSpecs.ParticleMaxLifetime = 1.5f;
        flameSpecs.MinVel              = glm::vec3(0.0f);
        flameSpecs.MaxVel              = glm::vec3(0.0f);

        auto flame                     = make_s<ParticleSystem>(flameSpecs, fireTexture, particleSystemLayout, pool);
        flame->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
        flame->RowOffset      = 0.0f;
        flame->RowCellSize    = 0.0833333333333333333333f;
        flame->ColumnCellSize = 0.166666666666666f;
        flame->ColumnOffset   = 0.0f;

        torchGroups.push_back({ torches[i], lights[i], sparks, flame });
    }

    ParticleSpecs ambientSpecs{};
    ambientSpecs.ParticleCount       = 500;
    ambientSpecs.EnableNoise         = true;
    ambientSpecs.TrailLength         = 0;
    ambientSpecs.SphereRadius        = 5.0f;
    ambientSpecs.ImmortalParticle    = true;
    ambientSpecs.ParticleSize        = 0.5f;
    ambientSpecs.EmitterPos          = glm::vec3(0, 2.0f, 0);
    ambientSpecs.ParticleMinLifetime = 5.0f;
    ambientSpecs.ParticleMaxLifetime = 10.0f;
    ambientSpecs.MinVel              = glm::vec3(-0.3f, -0.3f, -0.3f);
    ambientSpecs.MaxVel              = glm::vec3(0.3f, 0.3f, 0.3f);

    ambientParticles                 = make_s<ParticleSystem>(ambientSpecs, dustTexture, particleSystemLayout, pool);
    ambientParticles->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
}

void ForwardRenderer::CreateSwapchainRenderPass()
{
    RenderPass::AttachmentInfo colorAttachment{ _Context.GetSurface()->GetVKSurfaceFormat().format,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 0.0f, 0.0f, 0.0f, 0.0f } }; // Pass two clear values here if this is buggy.

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    RenderPass::CreateInfo HDRCreateInfo{ { colorAttachment }, { dep }, false, "Swapchain Final Pass (rename)" };

    _SwapchainRenderPass = std::make_unique<RenderPass>(_Context, HDRCreateInfo);
}

void ForwardRenderer::CreateHDRRenderPass()
{
    RenderPass::AttachmentInfo colorAttachment{
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 0.0f, 0.0f, 0.0f, 1.0f }
    }; // Pass two clear values here if this is buggy.

    RenderPass::AttachmentInfo depthAttachment{
        Utils::FindDepthFormat(), VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 1.0f, 0.0f }
    };

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    RenderPass::CreateInfo HDRCreateInfo{ { colorAttachment, depthAttachment }, { dep }, true, "HDR Render Pass" };

    _HDRRenderPass = std::make_unique<RenderPass>(_Context, HDRCreateInfo);
}
void ForwardRenderer::CreateShadowRenderPass()
{
    RenderPass::AttachmentInfo depthAttachment{
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 1.0f, 0.0f }
    };

    VkSubpassDependency dep{};
    dep.srcSubpass      = 0;
    dep.dstSubpass      = VK_SUBPASS_EXTERNAL;
    dep.srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPass::CreateInfo shadowRenderPassInfo{ { depthAttachment }, { dep }, true, "Directional Shadow Render Pass" };

    _ShadowMapRenderPass = std::make_unique<RenderPass>(_Context, shadowRenderPassInfo);
}

void ForwardRenderer::CreatePointShadowRenderPass()
{
    RenderPass::AttachmentInfo depthAttachment{
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 1.0f, 0.0f }
    };

    VkSubpassDependency dep{};
    dep.srcSubpass      = 0;
    dep.dstSubpass      = VK_SUBPASS_EXTERNAL;
    dep.srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dep.srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dep.dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.dstAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPass::CreateInfo shadowRenderPassInfo{ { depthAttachment }, { dep }, true, "Point Light Shadow Render Pass" };

    _PointShadowRenderPass = std::make_unique<RenderPass>(_Context, shadowRenderPassInfo);
}

void ForwardRenderer::EnableDepthOfField()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);

    finalPassSampler = Utils::CreateSampler(bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
// Connects the bloom image to the final render pass.
void ForwardRenderer::DisableDepthOfField()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);

    finalPassSampler =
        Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ForwardRenderer::SetupBokehPassPipeline()
{
    bokehPassPipeline = PipelineBuilder(_Context)
                            .SetRenderPass(bokehRenderPass->GetHandle())
                            .SetDescriptorSetLayout(bokehPassLayout)
                            .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                            .SetFragmentShader("assets/shaders/bokehPassFRAG.spv")
                            .SetDepthTest(VK_FALSE)
                            .SetDepthWrite(VK_FALSE)
                            .SetFixedViewport(bokehPassFramebuffer->GetWidth(), bokehPassFramebuffer->GetHeight())
                            .Build();
}
void ForwardRenderer::CreateBokehRenderPass()
{
    RenderPass::AttachmentInfo colorAttachment{
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 0.0f, 0.0f, 0.0f, 1.0f }
    };

    VkSubpassDependency dep{};
    dep.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass      = 0;
    dep.srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask   = VK_ACCESS_SHADER_READ_BIT;
    dep.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    dep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    RenderPass::CreateInfo createInfo{ { colorAttachment }, { dep }, false, "Bokeh Pass" };

    bokehRenderPass = std::make_unique<RenderPass>(_Context, createInfo);
}

void ForwardRenderer::CreateSwapchainFramebuffers()
{
    if (_SwapchainFramebuffers.size() > 0)
    {
        _SwapchainFramebuffers.clear();
    }

    for (auto imageView : _Swapchain->GetImageViews())
    {
        // Creating a framebuffer for each swapchain image. The depth image is
        // shared across the images.
        std::vector<VkImageView> attachments = {
            imageView,
        };
        _SwapchainFramebuffers.push_back(
            make_s<Framebuffer>(_SwapchainRenderPass->GetHandle(), attachments, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height));
    }
}

void ForwardRenderer::CreateHDRFramebuffer()
{
    HDRColorImage = make_s<Image>(
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ImageType::COLOR);

    HDRDepthImage = make_s<Image>(
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        Utils::FindDepthFormat(),
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ImageType::DEPTH);

    std::vector<VkImageView> attachments = { HDRColorImage->GetImageView(), HDRDepthImage->GetImageView() };

    _HDRFramebuffer =
        make_s<Framebuffer>(_HDRRenderPass->GetHandle(), attachments, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height);
}

void ForwardRenderer::CreateBokehFramebuffer()
{
    bokehPassImage = make_s<Image>(
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ImageType::COLOR);

    std::vector<VkImageView> attachments = { bokehPassImage->GetImageView() };

    bokehPassFramebuffer =
        make_s<Framebuffer>(bokehRenderPass->GetHandle(), attachments, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height);
}

void ForwardRenderer::Cleanup()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CommandBuffer::FreeCommandBuffer(cmdBuffers[i], cmdPool, _Context.GetDevice()->GetGraphicsQueue());
    }
    CommandBuffer::DestroyCommandPool(cmdPool);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassSceneSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassDepthSampler, nullptr);
    vkFreeMemory(_Context.GetDevice()->GetVKDevice(), globalParametersUBOBufferMemory, nullptr);
    vkDestroyBuffer(_Context.GetDevice()->GetVKDevice(), globalParametersUBOBuffer, nullptr);

    ImGui_ImplVulkan_DestroyFontUploadObjects();

    vkDestroyDescriptorPool(_Context.GetDevice()->GetVKDevice(), imguiPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
}

void ForwardRenderer::SyncLightsToUBO()
{
    // Directional light
    globalParametersUBO.dirLightPos               = glm::vec4(directionalLight.GetPosition(), 1.0f);
    globalParametersUBO.directionalLightIntensity = glm::vec4(directionalLight.GetIntensity());

    // Point lights — torches first, then standalone
    int pointIndex = 0;

    for (int i = 0; i < torchLights.size() && pointIndex < MAX_POINT_LIGHT_COUNT; i++, pointIndex++)
    {
        auto& light                                           = torchLights[i];
        globalParametersUBO.pointLightPositions[pointIndex]   = glm::vec4(light.GetPosition(), 1.0f);
        globalParametersUBO.pointLightColors[pointIndex]      = glm::vec4(light.GetColor(), 1.0f);
        globalParametersUBO.pointLightIntensities[pointIndex] = glm::vec4(light.GetIntensity());
    }

    // Red light
    if (pointIndex < MAX_POINT_LIGHT_COUNT)
    {
        globalParametersUBO.pointLightPositions[pointIndex]   = glm::vec4(redLight.GetPosition(), 1.0f);
        globalParametersUBO.pointLightColors[pointIndex]      = glm::vec4(redLight.GetColor(), 1.0f);
        globalParametersUBO.pointLightIntensities[pointIndex] = glm::vec4(redLight.GetIntensity());
        pointIndex++;
    }

    globalParametersUBO.pointLightCount = glm::vec4(static_cast<float>(pointIndex));
}

void ForwardRenderer::UpdateViewport_Scissor()
{
    _DynamicViewport.x            = 0.0f;
    _DynamicViewport.y            = 0.0f;
    _DynamicViewport.width        = (float)_Context.GetSurface()->GetVKExtent().width;
    _DynamicViewport.height       = (float)_Context.GetSurface()->GetVKExtent().height;
    _DynamicViewport.minDepth     = 0.0f;
    _DynamicViewport.maxDepth     = 1.0f;

    _DynamicScissor.offset        = { 0, 0 };
    _DynamicScissor.extent.width  = _Context.GetSurface()->GetVKExtent().width;
    _DynamicScissor.extent.height = _Context.GetSurface()->GetVKExtent().height;
}

void ForwardRenderer::InitImGui()
{
    VkDescriptorPoolSize pool_sizes[]    = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
                                             { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
                                             { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
                                             { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
                                             { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
                                             { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags                      = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets                    = 1000;
    pool_info.poolSizeCount              = std::size(pool_sizes);
    pool_info.pPoolSizes                 = pool_sizes;

    ENSURE(vkCreateDescriptorPool(_Context.GetDevice()->GetVKDevice(), &pool_info, nullptr, &imguiPool) == VK_SUCCESS, "Failed to initialize imgui pool");

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(_Context.GetWindow()->GetNativeWindow(), true);

    init_info.Instance       = _Context.GetInstance()->GetVkInstance();
    init_info.PhysicalDevice = _Context.GetPhysicalDevice()->GetVKPhysicalDevice();
    init_info.Device         = _Context.GetDevice()->GetVKDevice();
    init_info.Queue          = _Context.GetDevice()->GetGraphicsQueue();
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount  = 3;
    init_info.ImageCount     = 3;
    init_info.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, _SwapchainRenderPass->GetHandle());

    // Loading a custom font here and scaling the the font so that it can work
    // on a 4K display. TO DO: You should dynamically handle the DPI of the
    // monitor that the application is running on rather than setting a fixed
    // scale for the font.
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(
        (std::string(SOLUTION_DIR) +
         "Engine/assets/resources/Open_Sans/static/OpenSans/"
         "OpenSans-Regular.ttf")
            .c_str(),
        30);

    VkCommandBuffer singleCmdBuffer;
    VkCommandPool   singleCmdPool;
    CommandBuffer::CreateCommandBufferPool(_Context._QueueFamilies.TransferFamily, singleCmdPool);
    CommandBuffer::CreateCommandBuffer(singleCmdBuffer, singleCmdPool);
    CommandBuffer::BeginRecording(singleCmdBuffer);

    ImGui_ImplVulkan_CreateFontsTexture(singleCmdBuffer);

    CommandBuffer::EndRecording(singleCmdBuffer);
    CommandBuffer::Submit(singleCmdBuffer, _Context.GetDevice()->GetTransferQueue());
    CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool, _Context.GetDevice()->GetTransferQueue());
    CommandBuffer::DestroyCommandPool(singleCmdPool);
}

void ForwardRenderer::RenderFrame(const float InDeltaTime)
{
    _DeltaTime = InDeltaTime;

    // Flicker torch light intensities
    lightFlickerRate -= _DeltaTime;
    if (lightFlickerRate <= 0.0f)
    {
        lightFlickerRate = 0.1f;
        std::random_device                     rd;
        std::mt19937                           gen(rd());
        std::uniform_real_distribution<double> distributions[] = {
            std::uniform_real_distribution<double>(12.5, 25.0),
            std::uniform_real_distribution<double>(37.5, 50.0),
            std::uniform_real_distribution<double>(6.25, 12.5),
            std::uniform_real_distribution<double>(12.5, 25.0),
        };

        for (int i = 0; i < torchLights.size(); i++)
        {
            torchLights[i].SetIntensity(distributions[i](gen));
        }
    }

    // Sync all light data to the UBO
    SyncLightsToUBO();

    // Begin command buffer recording.
    CommandBuffer::BeginRecording(cmdBuffers[GFrameSync.CurrentBufferIndex]);

    // Timer.
    timer += 7.0f * _DeltaTime;

    auto  now  = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(now - startTime).count();

    // Update model matrices here.
    helmet.Rotate(2.0f * _DeltaTime, glm::vec3(0, 1, 0));

    // Update the particle systems.
    for (auto& group : torchGroups)
    {
        group.Sparks->UpdateParticles(_DeltaTime);
        group.Flame->UpdateParticles(_DeltaTime);
    }

    // Animating the directional light
    glm::mat4 directionalLightMVP = directionalLightProjectionMatrix * glm::lookAt(directionalLight.GetPosition(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // General data.
    glm::mat4 cameraView = _Camera->GetViewMatrix();
    glm::mat4 cameraProj = _Camera->GetProjectionMatrix();
    glm::vec4 cameraPos  = glm::vec4(_Camera->GetPosition(), 1.0f);
    glm::mat4 mat        = sponza.GetTransform();
    glm::mat4 mat2       = helmet.GetTransform();

    // Update some of parts of the global UBO buffer
    globalParametersUBO.viewMatrix           = cameraView;
    globalParametersUBO.projMatrix           = cameraProj;
    globalParametersUBO.cameraPosition       = cameraPos;
    globalParametersUBO.dirLightPos          = glm::vec4(directionalLight.GetPosition(), 1);
    globalParametersUBO.directionalLightMVP  = directionalLightMVP;
    globalParametersUBO.viewportDimension    = glm::vec4(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, 0.0f, 0.0f);
    globalParametersUBO.DOFFramebufferSize.x = bokehPassFramebuffer->GetWidth();
    globalParametersUBO.DOFFramebufferSize.y = bokehPassFramebuffer->GetHeight();

    auto dirShadowMap = _Graph->CreateTexture("DirectionalShadowMap", directionalShadowMapImage->GetVKImage(), VK_FORMAT_D32_SFLOAT, SHADOW_DIM, SHADOW_DIM, false);
    std::vector<RGResource*> pointShadowArray;

    if (globalParametersUBO.enablePointLightShadows.x == 1.0f)
    {
        // Directional Shadow Pass ////////////////////////////////////// RDG
        _Graph->AddPass(
            "DirectionalShadow",
            [&](RGPass& pass)
            {
                pass.Writes     = { dirShadowMap };

                pass.RecordFunc = [&](VkCommandBuffer cmd)
                {
                    _ShadowMapRenderPass->Begin(cmd, *_DirectionalShadowMapFramebuffer, "Directional Shadow Pass");

                    CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline);

                    CommandBuffer::PushConstants(cmd, shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
                    sponza.GetModel()->DrawIndexed(cmd, shadowPassPipeline->GetPipelineLayout());

                    CommandBuffer::PushConstants(cmd, shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
                    helmet.GetModel()->DrawIndexed(cmd, shadowPassPipeline->GetPipelineLayout());

                    _ShadowMapRenderPass->End(cmd);
                };
            });
        // ~Directional Shadow Pass ////////////////////////////////////// RDG

        for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
        {
            auto* res = _Graph->CreateTexture(
                "PointShadow_" + std::to_string(i),
                pointShadowMaps[i]->GetVKImage(),
                VK_FORMAT_D32_SFLOAT,
                POUNT_SHADOW_DIM,
                POUNT_SHADOW_DIM,
                false // NOT transient for now (because real VkImages exist in engine)
            );
            pointShadowArray.push_back(res);
        }

        // Point Shadow Pass ////////////////////////////////////// RDG
        _Graph->AddPass(
            "PointShadow",
            [&](RGPass& pass)
            {
                pass.Writes     = pointShadowArray;
                pass.RecordFunc = [&](VkCommandBuffer cmd)
                {
                    for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
                    {
                        _PointShadowRenderPass->Begin(cmd, *_PointShadowMapFramebuffers[i], (std::string("Point Shadow Pass") + std::to_string(i)).c_str());

                        CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowPassPipeline);

                        glm::vec3 position = glm::vec3(
                            globalParametersUBO.pointLightPositions[i].x, globalParametersUBO.pointLightPositions[i].y, globalParametersUBO.pointLightPositions[i].z);

                        globalParametersUBO.shadowMatrices[i][0] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
                        globalParametersUBO.shadowMatrices[i][1] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
                        globalParametersUBO.shadowMatrices[i][2] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
                        globalParametersUBO.shadowMatrices[i][3] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
                        globalParametersUBO.shadowMatrices[i][4] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
                        globalParametersUBO.shadowMatrices[i][5] =
                            pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));

                        struct PC
                        {
                            glm::vec4 lightPos;
                            glm::vec4 farPlane;
                        };

                        glm::vec4 pointLightIndex = glm::vec4(i);

                        PC pc;
                        pc.lightPos = glm::vec4(position, 1.0f);
                        pc.farPlane = glm::vec4(pointFarPlane);

                        CommandBuffer::PushConstants(cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
                        CommandBuffer::PushConstants(
                            cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &pointLightIndex);
                        CommandBuffer::PushConstants(
                            cmd,
                            pointShadowPassPipeline->GetPipelineLayout(),
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            sizeof(glm::mat4) + sizeof(glm::vec4),
                            sizeof(glm::vec4) + sizeof(glm::vec4),
                            &pc);
                        sponza.GetModel()->DrawIndexed(cmd, pointShadowPassPipeline->GetPipelineLayout());

                        CommandBuffer::PushConstants(cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
                        CommandBuffer::PushConstants(
                            cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &pointLightIndex);
                        CommandBuffer::PushConstants(
                            cmd,
                            pointShadowPassPipeline->GetPipelineLayout(),
                            VK_SHADER_STAGE_FRAGMENT_BIT,
                            sizeof(glm::mat4) + sizeof(glm::vec4),
                            sizeof(glm::vec4) + sizeof(glm::vec4),
                            &pc);
                        helmet.GetModel()->DrawIndexed(cmd, pointShadowPassPipeline->GetPipelineLayout());

                        _PointShadowRenderPass->End(cmd);
                    }
                };
            });
        // ~Point Shadow Pass ////////////////////////////////////// RDG
    }

    // Copy the global UBO data from CPU to GPU.
    memcpy(mappedGlobalParametersModelUBOBuffer, &globalParametersUBO, sizeof(GlobalParametersUBO));

    // TO DO: The animation sprite sheet offsets are hardcoded here. We
    // could use a better system to automatically calculate these variables.
    aniamtionRate -= _DeltaTime * 1.0f;
    if (aniamtionRate <= 0)
    {
        aniamtionRate = 0.01388888f;
        currentAnimationFrame++;
        if (currentAnimationFrame > 72)
        {
            currentAnimationFrame = 0;
        }
    }

    int  ct   = 0;
    bool done = false;
    for (int i = 1; i <= 6; i++)
    {
        if (done)
            break;

        for (int j = 1; j <= 12; j++)
        {
            if (ct >= currentAnimationFrame)
            {
                for (auto& group : torchGroups)
                {
                    group.Flame->RowOffset    = 0.0833333333333333333333f * j;
                    group.Flame->ColumnOffset = 0.166666666666666f * i;
                }
                done = true;
                break;
            }
            ct++;
        }
        ct++;
    }

    _FrameCount++;

    auto outColorImageCloud = _Graph->CreateTexture(
        "CloudOutColorImage",
        _CloudPass->GetOutputColorImage(),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        false);

    auto outTranmisttanceImageCloud = _Graph->CreateTexture(
        "CloudOutTransmittanceImage",
        _CloudPass->GetOutputTransmittanceImage(),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        false);

    // Cloud Pass ////////////////////////////////////// RDG
    _Graph->AddPass(
        "Cloud Pass",
        [&](RGPass& pass)
        {
            pass.Writes     = { outColorImageCloud, outTranmisttanceImageCloud };

            pass.RecordFunc = [&](VkCommandBuffer cmd)
            {
                CloudPass::PushConstants pc{};
                pc.cameraPos   = globalParametersUBO.cameraPosition;
                pc.invViewProj = glm::inverse(globalParametersUBO.projMatrix * globalParametersUBO.viewMatrix);
                pc.lightDir    = glm::normalize(-globalParametersUBO.dirLightPos);
                pc.time        = time;
                pc.frameCount  = _FrameCount;
                _CloudPass->Record(cmd, pc);
            };
        });
    // ~Cloud Pass ////////////////////////////////////// RDG

    auto SceneColorHDR = _Graph->CreateTexture(
        "SceneColorHDR",
        HDRColorImage->GetVKImage(),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        true // sampled
    );

    auto SceneDepth = _Graph->CreateTexture(
        "SceneDepth", HDRDepthImage->GetVKImage(), VK_FORMAT_D32_SFLOAT, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, false);

    // HDR Pass ////////////////////////////////////// RDG
    _Graph->AddPass(
        "HDR Scene",
        [&](RGPass& pass)
        {
            pass.Reads = { dirShadowMap };
            pass.Reads.insert(pass.Reads.end(), pointShadowArray.begin(), pointShadowArray.end());

            pass.Writes     = { SceneColorHDR, SceneDepth }; // main output targets

            pass.RecordFunc = [&](VkCommandBuffer cmd)
            {
                _HDRRenderPass->Begin(cmd, *_HDRFramebuffer, "HDR Pass");
                //  Drawing the skybox.
                glm::mat4 skyBoxView = glm::mat4(glm::mat3(cameraView));
                skybox.GetMaterial()->Bind(cmd);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);

                CommandBuffer::PushConstants(cmd, skyboxPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
                skybox.GetModel()->Draw(cmd, skyboxPipeline->GetPipelineLayout());

                struct pushConst
                {
                    glm::mat4 modelMat;
                    glm::vec4 color;
                };

                pushConst lightCubePC;
                // Drawing the light cube.
                cube.SetPosition(redLight.GetPosition());
                lightCubePC.modelMat = cube.GetTransform();
                lightCubePC.color    = glm::vec4(4.5f, 1.0f, 1.0f, 1.0f);
                cube.GetMaterial()->Bind(cmd);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(cmd, cubePipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &lightCubePC);
                cube.GetModel()->Draw(cmd, cubePipeline->GetPipelineLayout());

                // Drawing the Sponza.
                sponza.GetMaterial()->Bind(cmd);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
                sponza.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                // Drawing the helmet.
                helmet.GetMaterial()->Bind(cmd);
                CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
                helmet.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                // Drawing 4 torches.
                for (int i = 0; i < torch.GetModel()->GetMeshCount(); i++)
                {
                    torch.GetMaterial()->Bind(cmd);
                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch.GetTransform());
                    torch.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    torch2.GetMaterial()->Bind(cmd);
                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch2.GetTransform());
                    torch2.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    torch3.GetMaterial()->Bind(cmd);
                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch3.GetTransform());
                    torch3.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    torch4.GetMaterial()->Bind(cmd);
                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch4.GetTransform());
                    torch4.GetModel()->DrawIndexed(cmd, pipeline->GetPipelineLayout());
                }

                pushConst swordPC;
                // Draw the emissive sword.
                swordPC.modelMat = sword.GetTransform();
                swordPC.color    = glm::vec4(0.1f, 3.0f, 0.1f, 1.0f);
                sword.GetMaterial()->Bind(cmd);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(
                    cmd, EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &swordPC);
                sword.GetModel()->DrawIndexed(cmd, EmissiveObjectPipeline->GetPipelineLayout());

                // Draw the particles systems.
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);

                glm::vec4 sparkBrigtness;
                sparkBrigtness.x = 5.0f;
                glm::vec4 flameBrigthness;
                flameBrigthness.x = 4.0f;
                glm::vec4 dustBrigthness;
                dustBrigthness.x = 1.0f;

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &sparkBrigtness);
                for (auto& group : torchGroups)
                {
                    group.Sparks->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                }

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &flameBrigthness);

                for (auto& group : torchGroups)
                {
                    group.Flame->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                }

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &dustBrigthness);
                ambientParticles->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

                _HDRRenderPass->End(cmd);
            };
        });
    // ~HDR Pass ////////////////////////////////////// RDG

    auto bloomOutput = _Graph->CreateTexture(
        "bloom output",
        bloomAgent->GetPostProcessedImage()->GetVKImage(),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        true);

    // Bloom Pass ////////////////////////////////////// RDG
    _Graph->AddPass(
        "Bloom",
        [&](RGPass& pass)
        {
            pass.Reads      = { SceneColorHDR, outColorImageCloud };
            pass.Writes     = { bloomOutput };

            pass.RecordFunc = [&](VkCommandBuffer cmd) { bloomAgent->ApplyBloom(cmd); };
        });
    // ~Bloom Pass ////////////////////////////////////// RDG

    auto SceneColorHDRDoF = _Graph->CreateTexture(
        "SceneColorHDRDoF",
        bokehPassImage->GetVKImage(),
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        true);

    if (enableDepthOfField)
    {
        // Bokeh Pass ////////////////////////////////////// RDG
        _Graph->AddPass(
            "Bokeh / DOF",
            [&](RGPass& pass)
            {
                pass.Reads      = { bloomOutput, SceneDepth };
                pass.Writes     = { SceneColorHDRDoF };

                pass.RecordFunc = [&](VkCommandBuffer cmd)
                {
                    bokehRenderPass->Begin(cmd, *bokehPassFramebuffer, "DOF Pass");

                    CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bokehPassPipeline);
                    vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                    vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);

                    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, bokehPassPipeline->GetPipelineLayout(), 0, 1, &bokehDescriptorSet, 0, nullptr);
                    vkCmdDraw(cmd, 3, 1, 0, 0);

                    bokehRenderPass->End(cmd);
                };
            });

        // ~ Bokeh Pass ////////////////////////////////////// RDG
    }

    auto swapchainOutput = _Graph->CreateTexture(
        "Swapchain Image ",
        _Swapchain->GetImages()[_CurrentSwapchainImageIndex],
        VK_FORMAT_R16G16B16A16_SFLOAT,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height,
        true);

    // Swapchain Pass ////////////////////////////////////// RDG
    _Graph->AddPass(
        "Swapchain / Present",
        [&](RGPass& pass)
        {
            pass.Reads      = { SceneColorHDRDoF };
            pass.Writes     = { swapchainOutput };

            pass.RecordFunc = [&](VkCommandBuffer cmd)
            {
                _SwapchainRenderPass->Begin(cmd, *_SwapchainFramebuffers[_CurrentSwapchainImageIndex], "Swapchain Pass");

                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline->GetPipelineLayout(), 0, 1, &finalPassDescriptorSet, 0, nullptr);
                vkCmdDraw(cmd, 3, 1, 0, 0);

                ImGui::Begin("Hello, world!");

                glm::vec3 dirPos = directionalLight.GetPosition();
                if (ImGui::DragFloat3("Directional Light", &dirPos.x, 0.1f, -50, 50))
                {
                    directionalLight.SetPosition(dirPos);
                }

                glm::vec3 helmetPos = helmet.GetPosition();
                if (ImGui::DragFloat3("Helmet", &helmetPos.x, 0.01f, -10, 10))
                {
                    helmet.SetPosition(helmetPos);
                }

                glm::vec3 swordPos = sword.GetPosition();
                if (ImGui::DragFloat3("Sword", &swordPos.x, 0.01f, -10, 10))
                {
                    sword.SetPosition(swordPos);
                }

                for (int i = 0; i < torchGroups.size(); i++)
                {
                    auto&     group = torchGroups[i];
                    glm::vec3 pos   = group.Torch->GetPosition();
                    if (ImGui::DragFloat3(("Torch " + std::to_string(i + 1)).c_str(), &pos.x, 0.01f, -10, 10))
                    {
                        group.Torch->SetPosition(pos);
                        group.Light->SetPosition(pos + glm::vec3(0.0f, 0.22f, 0.0f));
                        group.Sparks->SetEmitterPosition(pos + glm::vec3(0.0f, 0.22f, 0.0f));
                        group.Flame->SetEmitterPosition(pos + glm::vec3(0.0f, 0.28f, 0.0f));
                    }
                }

                glm::vec3 redPos = redLight.GetPosition();
                if (ImGui::DragFloat3("Red Light", &redPos.x, 0.01f, -10, 10))
                {
                    redLight.SetPosition(redPos);
                }

                if (ImGui::Checkbox("Point light shadows", &pointLightShadows))
                {
                    pointLightShadows ? globalParametersUBO.enablePointLightShadows.x = 1.0f : globalParametersUBO.enablePointLightShadows.x = 0.0f;
                }

                if (ImGui::Checkbox("Enable Depth of Field", &enableDepthOfField))
                {
                    enableDepthOfField ? EnableDepthOfField() : DisableDepthOfField();
                }

                if (ImGui::Checkbox("Show DOF focus", &showDOFFocus))
                {
                    showDOFFocus ? globalParametersUBO.showDOFFocus.x = 1.0f : globalParametersUBO.showDOFFocus.x = 0.0f;
                }

                ImGui::DragFloat("Focal Depth", &globalParametersUBO.focalDepth.x, 0.01f, -10, 10);
                ImGui::DragFloat("Focal Length", &globalParametersUBO.focalLength.x, 0.01f, -10, 10);
                ImGui::DragFloat("Fstop", &globalParametersUBO.fstop.x, 0.01f, -10, 10);

                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();

                ImGui::Render();

                ImDrawData* draw_data = ImGui::GetDrawData();
                ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);

                _SwapchainRenderPass->End(cmd);
            };
        });
    // ~Swapchain Pass ////////////////////////////////////// RDG

    if (!printedDependencies)
    {
        _Graph->DebugPrint();
        _Graph->Compile();
        printedDependencies = true;
    }

    _Graph->Execute(cmdBuffers[GFrameSync.CurrentBufferIndex]);

    CommandBuffer::EndRecording(cmdBuffers[GFrameSync.CurrentBufferIndex]);

    _Graph->Clear();
}

void ForwardRenderer::RenderImGui()
{
    // ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    // ImGui::Begin("Shaders:");
    //  bool breakFrame = false;
    //  for (const auto& entry : std::filesystem::directory_iterator((std::string(SOLUTION_DIR) +
    //  "/Engine/assets/shaders").c_str()))
    //{
    //      int         fullstopIndex  = entry.path().string().find_last_of('.');
    //      int         lastSlashIndex = entry.path().string().find_last_of('\\');
    //      std::string path           = entry.path().string();
    //      std::string extension;
    //      std::string shadersFolder = std::string(SOLUTION_DIR) + "Engine/assets/shaders/";
    //      if (fullstopIndex != std::string::npos)
    //      {
    //          extension = path.substr(fullstopIndex, path.length() - fullstopIndex);
    //      }
    //      if (extension == ".frag" || extension == ".vert")
    //      {
    //          std::string shaderPath = path.substr(lastSlashIndex + 1, path.length() - lastSlashIndex);
    //          std::string shaderName = path.substr(lastSlashIndex + 1, fullstopIndex - (lastSlashIndex + 1));
    //          ImGui::PushID(shaderPath.c_str());
    //          if (ImGui::Button("Compile"))
    //          {
    //              std::string extensionWithoutDot = extension.substr(1, extension.length() - 1);
    //              for (int i = 0; i < extensionWithoutDot.length(); i++)
    //              {
    //                  extensionWithoutDot[i] = toupper(extensionWithoutDot[i]);
    //              }
    //
    //              std::string outputName = shaderName + extensionWithoutDot + ".spv";
    //              std::string command    = std::string(SOLUTION_DIR) + "Engine/vendor/VULKAN/1.4.328.1/bin/glslc.exe " +
    //                  shadersFolder + shaderPath + " -o " + "shaders/" + outputName;
    //              int success = system(command.c_str());
    //
    //              ImGui::PopID();
    //              ImGui::End();
    //              ImGui::EndFrame();
    //              breakFrame = true;
    //              break;
    //          }
    //          ImGui::PopID();
    //          ImGui::SameLine();
    //          ImGui::Text(shaderPath.c_str());
    //      }
    //  }
    //
    //  if (breakFrame)
    //{
    //      vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    //      // WindowResize(); // TODO: THis crashes due to swapchain.
    //  }

    // ImGui::End();
}

bool ForwardRenderer::BeginFrame()
{
    auto device = _Context.GetDevice()->GetVKDevice();

    vkWaitForFences(device, 1, &GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex]);

    VkResult result;

    result = vkAcquireNextImageKHR(
        device, _Swapchain->GetHandle(), UINT64_MAX, GFrameSync.AcquireFinishedSemaphores[GFrameSync.CurrentBufferIndex], VK_NULL_HANDLE, &_CurrentSwapchainImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _Context.GetWindow()->IsWindowResized())
    {
        HandleWindowResize(result);
        return false;
    }

    ENSURE(result == VK_SUCCESS, "Failed to acquire next image.");
    return true;
}

void ForwardRenderer::HandleWindowResize(VkResult InResult)
{
    if (InResult == VK_ERROR_OUT_OF_DATE_KHR || _Context.GetWindow()->IsWindowResized() || InResult == VK_SUBOPTIMAL_KHR)
    {
        vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

        // Wait if the window is minimized.
        int width = 0, height = 0;
        glfwGetFramebufferSize(_Context.GetWindow()->GetNativeWindow(), &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(_Context.GetWindow()->GetNativeWindow(), &width, &height);
            glfwWaitEvents();
        }

        _Swapchain->Recreate();

        // WindowResize()
        UpdateViewport_Scissor();
        CreateSwapchainFramebuffers(); // Works?
        CreateHDRFramebuffer();

        // Experimental. TO DO: Carry this part into the post processing
        // pipeline. Do it like how you did with Bloom.
        // CreateBokehRenderPass();
        CreateBokehFramebuffer();
        SetupBokehPassPipeline();

        bloomAgent = make_s<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage, _CloudPass);

        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);
        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassDepthSampler, nullptr);
        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassSceneSampler, nullptr);

        finalPassSampler = Utils::CreateSampler(bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::UpdateDescriptorSet(finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bokehPassSceneSampler = Utils::CreateSampler(
            bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::UpdateDescriptorSet(
            bokehDescriptorSet, bokehPassSceneSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bokehPassDepthSampler =
            Utils::CreateSampler(HDRDepthImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::UpdateDescriptorSet(bokehDescriptorSet, bokehPassDepthSampler, HDRDepthImage->GetImageView(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        // ~WindowResize()

        _Context.GetWindow()->OnResize();
        _Camera->SetViewportSize(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height);
        ImGui::EndFrame();
    }
}

void ForwardRenderer::EndFrame()
{
    VkResult result;
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        _Camera->OnUpdate(_DeltaTime);
    }

    auto queue                        = _Context.GetDevice()->GetGraphicsQueue();
    auto swapchainHandle              = _Swapchain->GetHandle();

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo         submitInfo{};

    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &GFrameSync.AcquireFinishedSemaphores[GFrameSync.CurrentBufferIndex];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmdBuffers[GFrameSync.CurrentBufferIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &GFrameSync.RenderingCompleteSemaphores[_CurrentSwapchainImageIndex];

    ENSURE(vkQueueSubmit(queue, 1, &submitInfo, GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex]) == VK_SUCCESS, "Failed to submit draw command buffer!");

    VkSwapchainKHR   swapchain = swapchainHandle;
    VkPresentInfoKHR presentInfo{};

    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &GFrameSync.RenderingCompleteSemaphores[_CurrentSwapchainImageIndex];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain;
    presentInfo.pImageIndices      = &_CurrentSwapchainImageIndex;
    presentInfo.pResults           = nullptr;

    result                         = vkQueuePresentKHR(queue, &presentInfo);
    ENSURE(result == VK_SUCCESS, "Failed to present swap chain image!");
}
