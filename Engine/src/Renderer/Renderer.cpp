#include "Bloom.h"
#include "Camera.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Instance.h"
#include "Mesh.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "Renderer/RenderPass.h"
// #include "Pipeline.h"
#include "CloudPass.h"
#include "Renderer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"
#include "VulkanContext.h"
#include "Window.h"

#include <Curl.h>
#include <filesystem>
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

    // Set the point light colors here.
    globalParametersUBO.pointLightColors[0] = glm::vec4(0.97, 0.76, 0.46, 1.0);
    globalParametersUBO.pointLightColors[1] = glm::vec4(0.97, 0.76, 0.46, 1.0);
    globalParametersUBO.pointLightColors[2] = glm::vec4(0.97, 0.76, 0.46, 1.0);
    globalParametersUBO.pointLightColors[3] = glm::vec4(0.97, 0.76, 0.46, 1.0);
    globalParametersUBO.pointLightColors[4] = glm::vec4(1.0, 0.0, 0.0, 1.0);

    CurlNoise::SetCurlSettings(false, 4.0f, 6, 1.0, 0.0);
    pointShadowMaps.resize(globalParametersUBO.pointLightCount.x);
    _PointShadowMapFramebuffers.resize(globalParametersUBO.pointLightCount.x);

    std::vector<DescriptorSetBindingSpecs> hdrLayout{
        DescriptorSetBindingSpecs{
            Type::UNIFORM_BUFFER, sizeof(GlobalParametersUBO), 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_NORMAL, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 2 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 3 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_SHADOWMAP, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 4 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_POINTSHADOWMAP, UINT64_MAX, 5, VK_SHADER_STAGE_FRAGMENT_BIT, 5 },
    };

    std::vector<DescriptorSetBindingSpecs> SkyboxLayout{ DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, sizeof(glm::mat4), 1, VK_SHADER_STAGE_VERTEX_BIT, 0 },
                                                         DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_CUBEMAP, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 } };

    std::vector<DescriptorSetBindingSpecs> ParticleSystemLayout{
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 1, VK_SHADER_STAGE_VERTEX_BIT, 0 }, // Index 0
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }, // Index 3
    };

    std::vector<DescriptorSetBindingSpecs> SwapchainLayout{ DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0 } };

    std::vector<DescriptorSetBindingSpecs> EmissiveLayout{
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, sizeof(glm::mat4) * 2, 1, VK_SHADER_STAGE_VERTEX_BIT, 0 },
    };

    std::vector<DescriptorSetBindingSpecs> CubeLayout{
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, sizeof(glm::mat4) * 2, 1, VK_SHADER_STAGE_VERTEX_BIT, 0 },
    };

    std::vector<DescriptorSetBindingSpecs> BokehPassLayout{
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 },
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, sizeof(glm::vec4) * 7, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 2 },
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
    model = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/Sponza/scene.gltf",
        LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
        pool,
        PBRLayout,
        directionalShadowMapImage,
        pointShadowMaps);
    model->Scale(0.005f, 0.005f, 0.005f);

    for (int i = 0; i < model->GetMeshCount(); i++)
    {
        Utils::UpdateDescriptorSet(model->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
    }

    // Loading the model Malenia's Helmet.
    model2 = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/MaleniaHelmet/scene.gltf",
        LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
        pool,
        PBRLayout,
        directionalShadowMapImage,
        pointShadowMaps);
    model2->Translate(0.0, 2.0f, 0.0);
    model2->Rotate(90, 0, 1, 0);
    model2->Scale(0.7f, 0.7f, 0.7f);

    for (int i = 0; i < model2->GetMeshCount(); i++)
    {
        Utils::UpdateDescriptorSet(model2->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
    }

    torch = make_s<Model>(
        std::string(SOLUTION_DIR) + "Engine/assets/models/torch/scene.gltf",
        LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
        pool,
        PBRLayout,
        directionalShadowMapImage,
        pointShadowMaps);

    torch1modelMatrix = glm::translate(torch1modelMatrix, glm::vec3(2.450f, 1.3f, 0.810f));
    torch1modelMatrix = glm::scale(torch1modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
    torch1modelMatrix = glm::rotate(torch1modelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));

    torch2modelMatrix = glm::translate(torch2modelMatrix, glm::vec3(0.610f, 1.3f, -1.170f));
    torch2modelMatrix = glm::scale(torch2modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
    torch2modelMatrix = glm::rotate(torch2modelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));

    torch3modelMatrix = glm::translate(torch3modelMatrix, glm::vec3(0.610f, 1.3f, 0.81f));
    torch3modelMatrix = glm::scale(torch3modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
    torch3modelMatrix = glm::rotate(torch3modelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));

    torch4modelMatrix = glm::translate(torch4modelMatrix, glm::vec3(2.45f, 1.3f, -1.170f));
    torch4modelMatrix = glm::scale(torch4modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
    torch4modelMatrix = glm::rotate(torch4modelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));

    for (int i = 0; i < torch->GetMeshCount(); i++)
    {
        Utils::UpdateDescriptorSet(torch->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
    }

    SetupParticleSystems();

    // Set the positions of the point lights in the scene we have 4 torches.
    globalParametersUBO.pointLightPositions[0]    = glm::vec4(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[1]    = glm::vec4(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[2]    = glm::vec4(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[3]    = glm::vec4(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.cameraNearPlane           = glm::vec4(_Camera->GetNearClip());
    globalParametersUBO.cameraFarPlane            = glm::vec4(_Camera->GetFarClip());
    globalParametersUBO.focalDepth                = glm::vec4(1.5f);
    globalParametersUBO.focalLength               = glm::vec4(15.0f);
    globalParametersUBO.fstop                     = glm::vec4(6.0f);
    globalParametersUBO.pointLightPositions[4]    = glm::vec4(-0.3f, 3.190, -0.180, 1.0f);
    globalParametersUBO.pointLightIntensities[4]  = glm::vec4(50.0f);
    globalParametersUBO.directionalLightIntensity = glm::vec4(10.0);
    globalParametersUBO.pointFarPlane             = glm::vec4(pointFarPlane);

    model3 = make_s<Model>(Utils::NormalizePath(std::string(SOLUTION_DIR) + "Engine/assets/models/sword/scene.gltf"), LOAD_VERTEX_POSITIONS, pool, emissiveLayout);
    model3->Translate(-2, 7, 0);
    model3->Rotate(54, 0, 0, 1);
    model3->Rotate(90, 0, 1, 0);
    model3->Scale(0.7f, 0.7f, 0.7f);

    for (int i = 0; i < model3->GetMeshCount(); i++)
    {
        Utils::UpdateDescriptorSet(model3->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(glm::mat4) * 2, 0);
    }

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
    skybox = make_s<Model>(cubeVertices, vertexCount, cubemap, pool, skyboxLayout);
    Utils::UpdateDescriptorSet(skybox->GetMeshes()[0]->GetDescriptorSet(), globalParametersUBOBuffer, sizeof(glm::mat4), sizeof(glm::mat4), 0);

    // A cube model to depict/debug point lights.
    cube = make_s<Model>(cubeVertices, vertexCount, nullptr, pool, cubeLayout);

    Utils::UpdateDescriptorSet(cube->GetMeshes()[0]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(glm::mat4) + sizeof(glm::mat4), 0);

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

    ParticleSpecs specs{};
    specs.ParticleCount       = 10;
    specs.EnableNoise         = true;
    specs.TrailLength         = 2;
    specs.SphereRadius        = 0.05f;
    specs.ImmortalParticle    = false;
    specs.ParticleSize        = 0.5f;
    specs.EmitterPos          = glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f);
    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 3.0f;
    specs.MinVel              = glm::vec3(-1.0f, 0.1f, -1.0f);
    specs.MaxVel              = glm::vec3(1.0f, 2.0f, 1.0f);

    fireSparks                = make_s<ParticleSystem>(specs, particleTexture, particleSystemLayout, pool);
    fireSparks->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 1.5f;
    specs.SphereRadius        = 0.0f;
    specs.TrailLength         = 0;
    specs.ParticleCount       = 1;
    specs.ImmortalParticle    = true;
    specs.ParticleSize        = 13.0f;
    specs.EnableNoise         = false;
    specs.EmitterPos          = glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.28f, torch1modelMatrix[3].z - 0.03f);
    specs.MinVel              = glm::vec4(0.0f);
    specs.MaxVel              = glm::vec4(0.0f);

    fireBase                  = make_s<ParticleSystem>(specs, fireTexture, particleSystemLayout, pool);
    fireBase->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    fireBase->RowOffset       = 0.0f;
    fireBase->RowCellSize     = 0.0833333333333333333333f;
    fireBase->ColumnCellSize  = 0.166666666666666f;
    fireBase->ColumnOffset    = 0.0f;

    specs.ParticleCount       = 10;
    specs.EnableNoise         = true;
    specs.TrailLength         = 2;
    specs.SphereRadius        = 0.05f;
    specs.ImmortalParticle    = false;
    specs.ParticleSize        = 0.5f;
    specs.EmitterPos          = glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f);
    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 3.0f;
    specs.MinVel              = glm::vec3(-1.0f, 0.1f, -1.0f);
    specs.MaxVel              = glm::vec3(1.0f, 2.0f, 1.0f);

    fireSparks2               = make_s<ParticleSystem>(specs, particleTexture, particleSystemLayout, pool);
    fireSparks2->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 1.5f;
    specs.SphereRadius        = 0.0f;
    specs.TrailLength         = 0;
    specs.ParticleCount       = 1;
    specs.ImmortalParticle    = true;
    specs.ParticleSize        = 13.0f;
    specs.EnableNoise         = false;
    specs.EmitterPos          = glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.28f, torch2modelMatrix[3].z + 0.03f);
    specs.MinVel              = glm::vec4(0.0f);
    specs.MaxVel              = glm::vec4(0.0f);

    fireBase2                 = make_s<ParticleSystem>(specs, fireTexture, particleSystemLayout, pool);
    fireBase2->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    fireBase2->RowOffset      = 0.0f;
    fireBase2->RowCellSize    = 0.0833333333333333333333f;
    fireBase2->ColumnCellSize = 0.166666666666666f;
    fireBase2->ColumnOffset   = 0.0f;

    specs.ParticleCount       = 10;
    specs.EnableNoise         = true;
    specs.TrailLength         = 2;
    specs.SphereRadius        = 0.05f;
    specs.ImmortalParticle    = false;
    specs.ParticleSize        = 0.5f;
    specs.EmitterPos          = glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f);
    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 3.0f;
    ;
    specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
    specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

    fireSparks3  = make_s<ParticleSystem>(specs, particleTexture, particleSystemLayout, pool);
    fireSparks3->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 1.5f;
    specs.SphereRadius        = 0.0f;
    specs.TrailLength         = 0;
    specs.ParticleCount       = 1;
    specs.ImmortalParticle    = true;
    specs.ParticleSize        = 13.0f;
    specs.EnableNoise         = false;
    specs.EmitterPos          = glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.28f, torch3modelMatrix[3].z - 0.03f);
    specs.MinVel              = glm::vec4(0.0f);
    specs.MaxVel              = glm::vec4(0.0f);

    fireBase3                 = make_s<ParticleSystem>(specs, fireTexture, particleSystemLayout, pool);
    fireBase3->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    fireBase3->RowOffset      = 0.0f;
    fireBase3->RowCellSize    = 0.0833333333333333333333f;
    fireBase3->ColumnCellSize = 0.166666666666666f;
    fireBase3->ColumnOffset   = 0.0f;

    specs.ParticleCount       = 10;
    specs.EnableNoise         = true;
    specs.TrailLength         = 2;
    specs.SphereRadius        = 0.05f;
    specs.ImmortalParticle    = false;
    specs.ParticleSize        = 0.5f;
    specs.EmitterPos          = glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f);
    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 3.0f;
    specs.MinVel              = glm::vec3(-1.0f, 0.1f, -1.0f);
    specs.MaxVel              = glm::vec3(1.0f, 2.0f, 1.0f);

    fireSparks4               = make_s<ParticleSystem>(specs, particleTexture, particleSystemLayout, pool);
    fireSparks4->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

    specs.ParticleMinLifetime = 0.1f;
    specs.ParticleMaxLifetime = 1.5f;
    specs.SphereRadius        = 0.0f;
    specs.TrailLength         = 0;
    specs.ParticleCount       = 1;
    specs.ImmortalParticle    = true;
    specs.ParticleSize        = 13.0f;
    specs.EnableNoise         = false;
    specs.EmitterPos          = glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.28f, torch4modelMatrix[3].z + 0.03f);
    specs.MinVel              = glm::vec4(0.0f);
    specs.MaxVel              = glm::vec4(0.0f);

    fireBase4                 = make_s<ParticleSystem>(specs, fireTexture, particleSystemLayout, pool);
    fireBase4->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    fireBase4->RowOffset      = 0.0f;
    fireBase4->RowCellSize    = 0.0833333333333333333333f;
    fireBase4->ColumnCellSize = 0.166666666666666f;
    fireBase4->ColumnOffset   = 0.0f;

    specs.ParticleCount       = 500;
    specs.EnableNoise         = true;
    specs.TrailLength         = 0;
    specs.SphereRadius        = 5.0f;
    specs.ImmortalParticle    = true;
    specs.ParticleSize        = 0.5f;
    specs.EmitterPos          = glm::vec3(0, 2.0f, 0);
    specs.ParticleMinLifetime = 5.0f;
    specs.ParticleMaxLifetime = 10.0f;
    specs.MinVel              = glm::vec3(-0.3f, -0.3f, -0.3f);
    specs.MaxVel              = glm::vec3(0.3f, 0.3f, 0.3f);

    ambientParticles          = make_s<ParticleSystem>(specs, dustTexture, particleSystemLayout, pool);
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
    // Begin command buffer recording.
    CommandBuffer::BeginRecording(cmdBuffers[GFrameSync.CurrentBufferIndex]);

    // Timer.
    timer += 7.0f * _DeltaTime;

    auto  now  = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(now - startTime).count();

    // Update model matrices here.
    model2->Rotate(2.0f * _DeltaTime, 0, 1, 0);

    // Update the particle systems.
    fireSparks->UpdateParticles(_DeltaTime);
    fireSparks2->UpdateParticles(_DeltaTime);
    fireSparks3->UpdateParticles(_DeltaTime);
    fireSparks4->UpdateParticles(_DeltaTime);
    fireBase->UpdateParticles(_DeltaTime);
    fireBase2->UpdateParticles(_DeltaTime);
    fireBase3->UpdateParticles(_DeltaTime);
    fireBase4->UpdateParticles(_DeltaTime);
    ambientParticles->UpdateParticles(_DeltaTime);

    // Animating the directional light
    glm::mat4 directionalLightMVP = directionalLightProjectionMatrix *
        glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // General data.
    glm::mat4 cameraView = _Camera->GetViewMatrix();
    glm::mat4 cameraProj = _Camera->GetProjectionMatrix();
    glm::vec4 cameraPos  = glm::vec4(_Camera->GetPosition(), 1.0f);
    glm::mat4 mat        = model->GetTransform();
    glm::mat4 mat2       = model2->GetTransform();

    // Update some of parts of the global UBO buffer
    globalParametersUBO.viewMatrix           = cameraView;
    globalParametersUBO.projMatrix           = cameraProj;
    globalParametersUBO.cameraPosition       = cameraPos;
    globalParametersUBO.dirLightPos          = directionalLightPosition;
    globalParametersUBO.directionalLightMVP  = directionalLightMVP;
    globalParametersUBO.viewportDimension    = glm::vec4(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, 0.0f, 0.0f);
    globalParametersUBO.DOFFramebufferSize.x = bokehPassFramebuffer->GetWidth();
    globalParametersUBO.DOFFramebufferSize.y = bokehPassFramebuffer->GetHeight();

    // Update point light positions. (Connected to the torch models.)
    globalParametersUBO.pointLightPositions[0] = glm::vec4(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[1] = glm::vec4(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[2] = glm::vec4(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[3] = glm::vec4(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f), 1.0f);

    // Update Particle system positions. (Connected to the torch models)
    fireSparks->SetEmitterPosition(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f));
    fireSparks2->SetEmitterPosition(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f));
    fireSparks3->SetEmitterPosition(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f));
    fireSparks4->SetEmitterPosition(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f));

    fireBase->SetEmitterPosition(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.28f, torch1modelMatrix[3].z - 0.03f));
    fireBase2->SetEmitterPosition(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.28f, torch2modelMatrix[3].z + 0.03f));
    fireBase3->SetEmitterPosition(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.28f, torch3modelMatrix[3].z - 0.03f));
    fireBase4->SetEmitterPosition(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.28f, torch4modelMatrix[3].z + 0.03f));

    lightFlickerRate -= _DeltaTime * 1.0f;

    if (lightFlickerRate <= 0.0f)
    {
        lightFlickerRate = 0.1f;
        std::random_device               rd;
        std::mt19937                     gen(rd());
        std::uniform_real_distribution<> distr(25.0f, 50.0f);
        std::uniform_real_distribution<> distr2(75.0f, 100.0f);
        std::uniform_real_distribution<> distr3(12.5f, 25.0f);
        std::uniform_real_distribution<> distr4(25.0f, 50.0f);
        globalParametersUBO.pointLightIntensities[0] = glm::vec4(distr(gen) / 2);
        globalParametersUBO.pointLightIntensities[1] = glm::vec4(distr2(gen) / 2);
        globalParametersUBO.pointLightIntensities[2] = glm::vec4(distr3(gen) / 2);
        globalParametersUBO.pointLightIntensities[3] = glm::vec4(distr4(gen) / 2);
        globalParametersUBO.pointLightIntensities[4] = glm::vec4(500.0f);
    }

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
                    model->DrawIndexed(cmd, shadowPassPipeline->GetPipelineLayout());

                    CommandBuffer::PushConstants(cmd, shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
                    model2->DrawIndexed(cmd, shadowPassPipeline->GetPipelineLayout());

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
                        model->DrawIndexed(cmd, pointShadowPassPipeline->GetPipelineLayout());

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
                        model2->DrawIndexed(cmd, pointShadowPassPipeline->GetPipelineLayout());

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
                fireBase->RowOffset     = 0.0833333333333333333333f * j;
                fireBase->ColumnOffset  = 0.166666666666666f * i;

                fireBase2->RowOffset    = 0.0833333333333333333333f * j;
                fireBase2->ColumnOffset = 0.166666666666666f * i;

                fireBase3->RowOffset    = 0.0833333333333333333333f * j;
                fireBase3->ColumnOffset = 0.166666666666666f * i;

                fireBase4->RowOffset    = 0.0833333333333333333333f * j;
                fireBase4->ColumnOffset = 0.166666666666666f * i;
                done                    = true;
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
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);

                CommandBuffer::PushConstants(cmd, skyboxPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
                skybox->Draw(cmd, skyboxPipeline->GetPipelineLayout());

                struct pushConst
                {
                    glm::mat4 modelMat;
                    glm::vec4 color;
                };

                pushConst lightCubePC;
                // Drawing the light cube.
                glm::mat4 lightCubeMat = glm::mat4(1.0f);
                lightCubeMat           = glm::translate(
                    lightCubeMat,
                    glm::vec3(globalParametersUBO.pointLightPositions[4].x, globalParametersUBO.pointLightPositions[4].y, globalParametersUBO.pointLightPositions[4].z));
                lightCubeMat         = glm::scale(lightCubeMat, glm::vec3(0.05f));
                lightCubePC.modelMat = lightCubeMat;
                lightCubePC.color    = glm::vec4(4.5f, 1.0f, 1.0f, 1.0f);
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(cmd, cubePipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &lightCubePC);
                cube->Draw(cmd, cubePipeline->GetPipelineLayout());

                // Drawing the Sponza.
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
                model->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                // Drawing the helmet.
                CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
                model2->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                // Drawing 4 torches.
                for (int i = 0; i < torch->GetMeshCount(); i++)
                {
                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch1modelMatrix);
                    torch->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch2modelMatrix);
                    torch->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch3modelMatrix);
                    torch->DrawIndexed(cmd, pipeline->GetPipelineLayout());

                    CommandBuffer::PushConstants(cmd, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch4modelMatrix);
                    torch->DrawIndexed(cmd, pipeline->GetPipelineLayout());
                }

                pushConst swordPC;
                // Draw the emissive sword.
                swordPC.modelMat = model3->GetTransform();
                swordPC.color    = glm::vec4(0.1f, 3.0f, 0.1f, 1.0f);
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                CommandBuffer::PushConstants(
                    cmd, EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &swordPC);
                model3->DrawIndexed(cmd, EmissiveObjectPipeline->GetPipelineLayout());

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
                fireSparks->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireSparks2->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireSparks3->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireSparks4->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &flameBrigthness);
                fireBase->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireBase2->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireBase3->Draw(cmd, particleSystemPipeline->GetPipelineLayout());
                fireBase4->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

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

                ImGui::DragFloat3("Directional Light", &directionalLightPosition.x, 0.1f, -50, 50);

                float* p[3] = {
                    &model2->GetTransform()[3].x,
                    &model2->GetTransform()[3].y,
                    &model2->GetTransform()[3].z,
                };

                ImGui::DragFloat3("Helmet", *p, 0.01f, -10, 10);

                float* p2[3] = {
                    &model3->GetTransform()[3].x,
                    &model3->GetTransform()[3].y,
                    &model3->GetTransform()[3].z,
                };

                ImGui::DragFloat3("Sword", *p2, 0.01f, -10, 10);

                float* t[3] = {
                    &torch1modelMatrix[3].x,
                    &torch1modelMatrix[3].y,
                    &torch1modelMatrix[3].z,
                };

                ImGui::DragFloat3("Torch 1", *t, 0.01f, -10, 10);

                float* t2[3] = {
                    &torch2modelMatrix[3].x,
                    &torch2modelMatrix[3].y,
                    &torch2modelMatrix[3].z,
                };

                ImGui::DragFloat3("Torch 2", *t2, 0.01f, -10, 10);

                float* t3[3] = {
                    &torch3modelMatrix[3].x,
                    &torch3modelMatrix[3].y,
                    &torch3modelMatrix[3].z,
                };

                ImGui::DragFloat3("Torch 3", *t3, 0.01f, -10, 10);

                float* t4[3] = {
                    &torch4modelMatrix[3].x,
                    &torch4modelMatrix[3].y,
                    &torch4modelMatrix[3].z,
                };

                ImGui::DragFloat3("Torch 4", *t4, 0.01f, -10, 10);

                float* p3[3] = { &globalParametersUBO.pointLightPositions[4].x,
                                 &globalParametersUBO.pointLightPositions[4].y,
                                 &globalParametersUBO.pointLightPositions[4].z };

                ImGui::DragFloat3("point light", *p3, 0.01f, -10, 10);

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
