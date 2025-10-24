#include "Bloom.h"
#include "Camera.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Instance.h"
#include "LogicalDevice.h"
#include "Mesh.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "PhysicalDevice.h"
// #include "Pipeline.h"
#include "Renderer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"
#include "VulkanContext.h"
#include "Window.h"

#include <Curl.h>
#include <filesystem>
#include <iostream>

ForwardRenderer::ForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
    : _Context(InContext), _Swapchain(InSwapchain), _Camera(InCamera)
{
}

void ForwardRenderer::Init()
{
    CreateSynchronizationPrimitives();
    UpdateViewport_Scissor();

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
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER,
                                   sizeof(GlobalParametersUBO),
                                   1,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_NORMAL, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 2 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 3 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_SHADOWMAP, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 4 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_POINTSHADOWMAP, UINT64_MAX, 5, VK_SHADER_STAGE_FRAGMENT_BIT, 5 },
    };

    std::vector<DescriptorSetBindingSpecs> SkyboxLayout{
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER, sizeof(glm::mat4), 1, VK_SHADER_STAGE_VERTEX_BIT, 0 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_CUBEMAP, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }
    };

    std::vector<DescriptorSetBindingSpecs> ParticleSystemLayout{
        DescriptorSetBindingSpecs{ Type::UNIFORM_BUFFER,
                                   (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3),
                                   1,
                                   VK_SHADER_STAGE_VERTEX_BIT,
                                   0 }, // Index 0
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 1 }, // Index 3
    };

    std::vector<DescriptorSetBindingSpecs> SwapchainLayout{
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 0 },
        DescriptorSetBindingSpecs{ Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, VK_SHADER_STAGE_FRAGMENT_BIT, 2 },
    };

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

    // Create the pool(s) that we need here.
    pool = make_s<DescriptorPool>(
        200, std::vector<VkDescriptorType>{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER });

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
    vkMapMemory(
        _Context.GetDevice()->GetVKDevice(),
        globalParametersUBOBufferMemory,
        0,
        sizeof(GlobalParametersUBO),
        0,
        &mappedGlobalParametersModelUBOBuffer);

    // Create an image for the shadowmap. We will render to this image when
    // we are doing a shadow pass.
    directionalShadowMapImage = make_s<Image>(
        SHADOW_DIM,
        SHADOW_DIM,
        VK_FORMAT_D32_SFLOAT,
        (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT),
        ImageType::DEPTH);

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

    VkResult rslt = vkAllocateDescriptorSets(_Context.GetDevice()->GetVKDevice(), &allocInfo, &finalPassDescriptorSet);
    ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    // Allocate bokeh pass descriptor Set.
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &bokehPassLayout->GetDescriptorLayout();

    rslt                         = vkAllocateDescriptorSets(_Context.GetDevice()->GetVKDevice(), &allocInfo, &bokehDescriptorSet);
    ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

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

    _DirectionalShadowMapFramebuffer =
        make_s<Framebuffer>(_ShadowMapRenderPass->GetHandle(), attachments, SHADOW_DIM, SHADOW_DIM);

    // Framebuffers need for point light shadows. (Dependent on the number
    // of point lights in the scene)
    for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
    {
        attachments = { pointShadowMaps[i]->GetImageView() };

        _PointShadowMapFramebuffers[i] =
            make_s<Framebuffer>(_PointShadowRenderPass->GetHandle(), attachments, POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, 6);
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
        Utils::UpdateDescriptorSet(
            model->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
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
        Utils::UpdateDescriptorSet(
            model2->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
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
        Utils::UpdateDescriptorSet(
            torch->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(GlobalParametersUBO), 0);
    }

    SetupParticleSystems();

    // Set the positions of the point lights in the scene we have 4 torches.
    globalParametersUBO.pointLightPositions[0] =
        glm::vec4(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[1] =
        glm::vec4(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[2] =
        glm::vec4(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[3] =
        glm::vec4(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.cameraNearPlane           = glm::vec4(_Camera->GetNearClip());
    globalParametersUBO.cameraFarPlane            = glm::vec4(_Camera->GetFarClip());
    globalParametersUBO.focalDepth                = glm::vec4(1.5f);
    globalParametersUBO.focalLength               = glm::vec4(15.0f);
    globalParametersUBO.fstop                     = glm::vec4(6.0f);
    globalParametersUBO.pointLightPositions[4]    = glm::vec4(-0.3f, 3.190, -0.180, 1.0f);
    globalParametersUBO.pointLightIntensities[4]  = glm::vec4(50.0f);
    globalParametersUBO.directionalLightIntensity = glm::vec4(10.0);
    globalParametersUBO.pointFarPlane             = glm::vec4(pointFarPlane);

    model3                                        = make_s<Model>(
        Utils::NormalizePath(std::string(SOLUTION_DIR) + "Engine/assets/models/sword/scene.gltf"),
        LOAD_VERTEX_POSITIONS,
        pool,
        emissiveLayout);
    model3->Translate(-2, 7, 0);
    model3->Rotate(54, 0, 0, 1);
    model3->Rotate(90, 0, 1, 0);
    model3->Scale(0.7f, 0.7f, 0.7f);

    for (int i = 0; i < model3->GetMeshCount(); i++)
    {
        Utils::UpdateDescriptorSet(
            model3->GetMeshes()[i]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(glm::mat4) * 2, 0);
    }

    // Vertex data for the skybox.
    const uint32_t vertexCount               = 3 * 6 * 6;
    const float    cubeVertices[vertexCount] = {
        -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
        -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

        1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
        1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

        -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
        1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,
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
    Utils::UpdateDescriptorSet(
        skybox->GetMeshes()[0]->GetDescriptorSet(), globalParametersUBOBuffer, sizeof(glm::mat4), sizeof(glm::mat4), 0);

    // A cube model to depict/debug point lights.
    cube = make_s<Model>(cubeVertices, vertexCount, nullptr, pool, cubeLayout);

    Utils::UpdateDescriptorSet(
        cube->GetMeshes()[0]->GetDescriptorSet(), globalParametersUBOBuffer, 0, sizeof(glm::mat4) + sizeof(glm::mat4), 0);

    CommandBuffer::CreateCommandBufferPool(_Context._QueueFamilies.GraphicsFamily, cmdPool);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CommandBuffer::CreateCommandBuffer(cmdBuffers[i], cmdPool);
    }

    bloomAgent = make_s<Bloom>();
    bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage);

    finalPassSampler = Utils::CreateSampler(
        bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);

    Utils::UpdateDescriptorSet(
        finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassSceneSampler = Utils::CreateSampler(
        bloomAgent->GetPostProcessedImage(),
        ImageType::COLOR,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_FALSE);
    Utils::UpdateDescriptorSet(
        bokehDescriptorSet,
        bokehPassSceneSampler,
        bloomAgent->GetPostProcessedImage()->GetImageView(),
        0,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassDepthSampler = Utils::CreateSampler(
        HDRDepthImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        bokehDescriptorSet,
        bokehPassDepthSampler,
        HDRDepthImage->GetImageView(),
        1,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

    Utils::UpdateDescriptorSet(
        bokehDescriptorSet,
        globalParametersUBOBuffer,
        offsetof(GlobalParametersUBO, DOFFramebufferSize),
        sizeof(glm::vec4) * 7,
        2);
}

void ForwardRenderer::CreateSynchronizationPrimitives()
{
    // Create synchronization primitives
    if (m_RenderingCompleteSemaphores.size() > 0 || m_ImageAvailableSemaphores.size() > 0 || m_InFlightFences.size() > 0)
    {
        for (int i = 0; i < m_FramesInFlight; i++)
        {
            vkDestroySemaphore(_Context.GetDevice()->GetVKDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(_Context.GetDevice()->GetVKDevice(), m_RenderingCompleteSemaphores[i], nullptr);
            vkDestroyFence(_Context.GetDevice()->GetVKDevice(), m_InFlightFences[i], nullptr);
        }
    }

    // Initialize 2 sempahores and a single fence needed to synchronize
    // rendering and presentation.
    m_RenderingCompleteSemaphores.resize(m_FramesInFlight);
    m_ImageAvailableSemaphores.resize(m_FramesInFlight);
    m_InFlightFences.resize(m_FramesInFlight);

    // Setup the fences and semaphores needed to synchronize the rendering.
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags             = VK_FENCE_CREATE_SIGNALED_BIT;

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    // Create the scynhronization objects as many times as the frames in flight
    // number.
    for (int i = 0; i < m_FramesInFlight; i++)
    {
        ASSERT(
            vkCreateSemaphore(_Context.GetDevice()->GetVKDevice(), &semaphoreInfo, nullptr, &m_RenderingCompleteSemaphores[i]) ==
                VK_SUCCESS,
            "Failed to create rendering complete semaphore.");
        ASSERT(
            vkCreateFence(_Context.GetDevice()->GetVKDevice(), &fenceCreateInfo, nullptr, &m_InFlightFences[i]) == VK_SUCCESS,
            "Failed to create is rendering fence.");
        ASSERT(
            vkCreateSemaphore(_Context.GetDevice()->GetVKDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) ==
                VK_SUCCESS,
            "Failed to create image available semaphore.");
    }
}

void ForwardRenderer::SetupPBRPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = PBRLayout;
    specs.RenderPass              = _HDRRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/PBRShaderVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/PBRShaderFRAG.spv";

    VkPushConstantRange pcRange;
    pcRange.offset           = 0;
    pcRange.size             = sizeof(glm::mat4);
    pcRange.stageFlags       = VK_SHADER_STAGE_VERTEX_BIT;

    specs.PushConstantRanges = { pcRange };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription.binding               = 0;
    bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions.resize(5);
    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = 0;

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32_SFLOAT;
    attributeDescriptions[1].offset   = sizeof(glm::vec3);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset   = sizeof(glm::vec3) + sizeof(glm::vec2);

    attributeDescriptions[3].binding  = 0;
    attributeDescriptions[3].location = 3;
    attributeDescriptions[3].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[3].offset   = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3);

    attributeDescriptions[4].binding  = 0;
    attributeDescriptions[4].location = 4;
    attributeDescriptions[4].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[4].offset   = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3);

    specs.VertexBindings              = { bindingDescription };
    specs.VertexAttributes            = attributeDescriptions;

    pipeline                          = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::SetupFinalPassPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = swapchainLayout;
    specs.RenderPass              = _SwapchainRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/quadRenderVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/swapchainFRAG.spv";

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    finalPassPipeline                        = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::SetupShadowPassPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = PBRLayout;
    specs.RenderPass              = _ShadowMapRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 1.25f;
    specs.DepthBiasSlopeFactor    = 1.75f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = VK_TRUE;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/shadowPassVERT.spv";
    specs.ViewportHeight          = SHADOW_DIM;
    specs.ViewportWidth           = SHADOW_DIM;
    specs.EnableDynamicStates     = false;

    VkPushConstantRange pcRange;
    pcRange.offset           = 0;
    pcRange.size             = sizeof(glm::mat4);
    pcRange.stageFlags       = VK_SHADER_STAGE_VERTEX_BIT;

    specs.PushConstantRanges = { pcRange };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription2.binding              = 0;
    bindingDescription2.stride =
        sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
    bindingDescription2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions2.resize(1);

    attributeDescriptions2[0].binding  = 0;
    attributeDescriptions2[0].location = 0;
    attributeDescriptions2[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions2[0].offset   = 0;

    specs.VertexBindings               = { bindingDescription2 };
    specs.VertexAttributes             = attributeDescriptions2;

    shadowPassPipeline                 = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::SetupPointShadowPassPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = PBRLayout;
    specs.RenderPass              = _PointShadowRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 1.25f;
    specs.DepthBiasSlopeFactor    = 1.75f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = VK_FALSE;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/pointShadowPassVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/pointShadowPassFRAG.spv";
    specs.GeometryShaderPath      = "assets/shaders/pointShadowPassGEOM.spv";
    specs.ViewportHeight          = POUNT_SHADOW_DIM;
    specs.ViewportWidth           = POUNT_SHADOW_DIM;
    specs.EnableDynamicStates     = false;

    VkPushConstantRange pcRange;
    pcRange.offset     = 0;
    pcRange.size       = sizeof(glm::mat4);
    pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPushConstantRange pcRange2;
    pcRange2.offset     = sizeof(glm::mat4);
    pcRange2.size       = sizeof(glm::vec4);
    pcRange2.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

    VkPushConstantRange pcRange3;
    pcRange3.offset          = sizeof(glm::mat4) + sizeof(glm::vec4);
    pcRange3.size            = sizeof(glm::vec4) + sizeof(glm::vec4);
    pcRange3.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    specs.PushConstantRanges = { pcRange, pcRange2, pcRange3 };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription2.binding              = 0;
    bindingDescription2.stride =
        sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
    bindingDescription2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions2.resize(1);

    attributeDescriptions2[0].binding  = 0;
    attributeDescriptions2[0].location = 0;
    attributeDescriptions2[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions2[0].offset   = 0;

    specs.VertexBindings               = { bindingDescription2 };
    specs.VertexAttributes             = attributeDescriptions2;

    pointShadowPassPipeline            = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::SetupSkyboxPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = skyboxLayout;
    specs.RenderPass              = _HDRRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_FALSE;
    specs.EnableDepthWriting      = VK_FALSE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/cubemapVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/cubemapFRAG.spv";

    VkPushConstantRange pcRange;
    pcRange.offset           = 0;
    pcRange.size             = sizeof(glm::mat4);
    pcRange.stageFlags       = VK_SHADER_STAGE_VERTEX_BIT;

    specs.PushConstantRanges = { pcRange };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription3.binding              = 0;
    bindingDescription3.stride               = sizeof(glm::vec3);
    bindingDescription3.inputRate            = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions3.resize(1);
    // For position
    attributeDescriptions3[0].binding  = 0;
    attributeDescriptions3[0].location = 0;
    attributeDescriptions3[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions3[0].offset   = 0;

    specs.VertexBindings               = { bindingDescription3 };
    specs.VertexAttributes             = attributeDescriptions3;

    skyboxPipeline                     = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::SetupCubePipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = cubeLayout;
    specs.RenderPass              = _HDRRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_NONE;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/emissiveShaderVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/emissiveShaderFRAG.spv";

    VkPushConstantRange pcRange;
    pcRange.offset           = 0;
    pcRange.size             = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec4);
    pcRange.stageFlags       = VK_SHADER_STAGE_VERTEX_BIT;

    specs.PushConstantRanges = { pcRange };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_FALSE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription3.binding              = 0;
    bindingDescription3.stride               = sizeof(glm::vec3);
    bindingDescription3.inputRate            = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions3.resize(1);
    // For position
    attributeDescriptions3[0].binding  = 0;
    attributeDescriptions3[0].location = 0;
    attributeDescriptions3[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions3[0].offset   = 0;

    specs.VertexBindings               = { bindingDescription3 };
    specs.VertexAttributes             = attributeDescriptions3;

    cubePipeline                       = make_s<Pipeline>(_Context, specs);
}

void ForwardRenderer::SetupParticleSystemPipeline()
{
    Pipeline::Specs          particleSpecs{};
    Ref<DescriptorSetLayout> layout       = particleSystemLayout;
    particleSpecs.DescriptorSetLayout     = layout;
    particleSpecs.RenderPass              = _HDRRenderPass->GetHandle();
    particleSpecs.CullMode                = VK_CULL_MODE_BACK_BIT;
    particleSpecs.DepthBiasClamp          = 0.0f;
    particleSpecs.DepthBiasConstantFactor = 0.0f;
    particleSpecs.DepthBiasSlopeFactor    = 0.0f;
    particleSpecs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    particleSpecs.EnableDepthBias         = false;
    particleSpecs.EnableDepthTesting      = VK_TRUE;
    particleSpecs.EnableDepthWriting      = VK_FALSE;
    particleSpecs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    particleSpecs.PolygonMode             = VK_POLYGON_MODE_FILL;
    particleSpecs.VertexShaderPath        = "assets/shaders/particleVERT.spv";
    particleSpecs.FragmentShaderPath      = "assets/shaders/particleFRAG.spv";
    particleSpecs.PrimitiveTopology       = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPushConstantRange pcRange;
    pcRange.offset                   = 0;
    pcRange.size                     = sizeof(glm::vec4);
    pcRange.stageFlags               = VK_SHADER_STAGE_FRAGMENT_BIT;

    particleSpecs.PushConstantRanges = { pcRange };

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    particleSpecs.ColorBlendAttachmentState  = colorBlendAttachment;

    bindingDescription4.binding              = 0;
    bindingDescription4.stride               = sizeof(Particle);
    bindingDescription4.inputRate            = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions4.resize(9);
    attributeDescriptions4[0].binding  = 0;
    attributeDescriptions4[0].location = 0;
    attributeDescriptions4[0].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions4[0].offset   = offsetof(Particle, Position);

    attributeDescriptions4[1].binding  = 0;
    attributeDescriptions4[1].location = 1;
    attributeDescriptions4[1].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions4[1].offset   = offsetof(Particle, Color);

    attributeDescriptions4[2].binding  = 0;
    attributeDescriptions4[2].location = 2;
    attributeDescriptions4[2].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[2].offset   = offsetof(Particle, Alpha);

    attributeDescriptions4[3].binding  = 0;
    attributeDescriptions4[3].location = 3;
    attributeDescriptions4[3].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[3].offset   = offsetof(Particle, SizeRadius);

    attributeDescriptions4[4].binding  = 0;
    attributeDescriptions4[4].location = 4;
    attributeDescriptions4[4].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[4].offset   = offsetof(Particle, Rotation);

    attributeDescriptions4[5].binding  = 0;
    attributeDescriptions4[5].location = 5;
    attributeDescriptions4[5].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[5].offset   = offsetof(Particle, RowOffset);

    attributeDescriptions4[6].binding  = 0;
    attributeDescriptions4[6].location = 6;
    attributeDescriptions4[6].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[6].offset   = offsetof(Particle, ColumnOffset);

    attributeDescriptions4[7].binding  = 0;
    attributeDescriptions4[7].location = 7;
    attributeDescriptions4[7].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[7].offset   = offsetof(Particle, RowCellSize);

    attributeDescriptions4[8].binding  = 0;
    attributeDescriptions4[8].location = 8;
    attributeDescriptions4[8].format   = VK_FORMAT_R32_SFLOAT;
    attributeDescriptions4[8].offset   = offsetof(Particle, ColumnCellSize);

    particleSpecs.VertexBindings       = { bindingDescription4 };
    particleSpecs.VertexAttributes     = attributeDescriptions4;

    particleSystemPipeline             = make_s<Pipeline>(_Context, particleSpecs);
}
void ForwardRenderer::SetupEmissiveObjectPipeline()
{
    // Emissive object pipeline.
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout = emissiveLayout;
    specs.RenderPass          = _HDRRenderPass->GetHandle();
    specs.VertexShaderPath    = "assets/shaders/emissiveShaderVERT.spv";
    specs.FragmentShaderPath  = "assets/shaders/emissiveShaderFRAG.spv";

    VkPushConstantRange pcRange;
    pcRange.offset                = 0;
    pcRange.size                  = sizeof(glm::mat4) + sizeof(glm::vec4);
    pcRange.stageFlags            = VK_SHADER_STAGE_VERTEX_BIT;

    specs.PushConstantRanges      = { pcRange };

    specs.CullMode                = VK_CULL_MODE_BACK_BIT;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_TRUE;
    specs.EnableDepthWriting      = VK_TRUE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bindingDescription5.binding              = 0;
    bindingDescription5.stride               = sizeof(glm::vec3);
    bindingDescription5.inputRate            = VK_VERTEX_INPUT_RATE_VERTEX;

    attributeDescriptions5.resize(1);
    attributeDescriptions5[0].binding  = 0;
    attributeDescriptions5[0].location = 0;
    attributeDescriptions5[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions5[0].offset   = 0;

    specs.VertexBindings               = { bindingDescription5 };
    specs.VertexAttributes             = attributeDescriptions5;

    EmissiveObjectPipeline             = make_s<Pipeline>(_Context, specs);
}

void ForwardRenderer::SetupParticleSystems()
{
    particleTexture =
        make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/spark.png") }, VK_FORMAT_R8G8B8A8_SRGB);
    fireTexture = make_s<Image>(
        std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/fire_sprite_sheet.png") }, VK_FORMAT_R8G8B8A8_SRGB);
    dustTexture =
        make_s<Image>(std::vector{ (std::string(SOLUTION_DIR) + "Engine/assets/textures/dust.png") }, VK_FORMAT_R8G8B8A8_SRGB);

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
    RenderPass::AttachmentInfo colorAttachment{ VK_FORMAT_R16G16B16A16_SFLOAT,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 0.0f, 0.0f, 0.0f, 1.0f } }; // Pass two clear values here if this is buggy.

    RenderPass::AttachmentInfo depthAttachment{ Utils::FindDepthFormat(),
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 1.0f, 0.0f } };

    VkSubpassDependency dep{};
    dep.srcSubpass   = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass   = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dep.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dep.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    RenderPass::CreateInfo HDRCreateInfo{ { colorAttachment, depthAttachment }, { dep }, true, "HDR Render Pass" };

    _HDRRenderPass = std::make_unique<RenderPass>(_Context, HDRCreateInfo);
}
void ForwardRenderer::CreateShadowRenderPass()
{
    RenderPass::AttachmentInfo depthAttachment{ VK_FORMAT_D32_SFLOAT,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 1.0f, 0.0f } };

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
    RenderPass::AttachmentInfo depthAttachment{ VK_FORMAT_D32_SFLOAT,
                                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 1.0f, 0.0f } };

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

    finalPassSampler = Utils::CreateSampler(
        bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
// Connects the bloom image to the final render pass.
void ForwardRenderer::DisableDepthOfField()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);

    finalPassSampler = Utils::CreateSampler(
        bloomAgent->GetPostProcessedImage(),
        ImageType::COLOR,
        VK_FILTER_LINEAR,
        VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_FALSE);
    Utils::UpdateDescriptorSet(
        finalPassDescriptorSet,
        finalPassSampler,
        bloomAgent->GetPostProcessedImage()->GetImageView(),
        0,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void ForwardRenderer::SetupBokehPassPipeline()
{
    Pipeline::Specs specs{};
    specs.DescriptorSetLayout     = bokehPassLayout;
    specs.RenderPass              = bokehRenderPass->GetHandle();
    specs.CullMode                = VK_CULL_MODE_NONE;
    specs.DepthBiasClamp          = 0.0f;
    specs.DepthBiasConstantFactor = 0.0f;
    specs.DepthBiasSlopeFactor    = 0.0f;
    specs.DepthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
    specs.EnableDepthBias         = false;
    specs.EnableDepthTesting      = VK_FALSE;
    specs.EnableDepthWriting      = VK_FALSE;
    specs.FrontFace               = VK_FRONT_FACE_CLOCKWISE;
    specs.PolygonMode             = VK_POLYGON_MODE_FILL;
    specs.VertexShaderPath        = "assets/shaders/quadRenderVERT.spv";
    specs.FragmentShaderPath      = "assets/shaders/bokehPassFRAG.spv";
    specs.ViewportHeight          = bokehPassFramebuffer->GetHeight();
    specs.ViewportWidth           = bokehPassFramebuffer->GetWidth();

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable         = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;

    specs.ColorBlendAttachmentState          = colorBlendAttachment;

    bokehPassPipeline                        = make_s<Pipeline>(_Context, specs);
}
void ForwardRenderer::CreateBokehRenderPass()
{
    RenderPass::AttachmentInfo colorAttachment{ VK_FORMAT_R16G16B16A16_SFLOAT,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 0.0f, 0.0f, 0.0f, 1.0f } };

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
        _SwapchainFramebuffers.push_back(make_s<Framebuffer>(
            _SwapchainRenderPass->GetHandle(),
            attachments,
            _Context.GetSurface()->GetVKExtent().width,
            _Context.GetSurface()->GetVKExtent().height));
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

    _HDRFramebuffer                      = make_s<Framebuffer>(
        _HDRRenderPass->GetHandle(),
        attachments,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height);
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

    bokehPassFramebuffer                 = make_s<Framebuffer>(
        bokehRenderPass->GetHandle(),
        attachments,
        _Context.GetSurface()->GetVKExtent().width,
        _Context.GetSurface()->GetVKExtent().height);
}

void ForwardRenderer::Cleanup()
{
    for (int i = 0; i < m_FramesInFlight; i++)
    {
        vkDestroySemaphore(_Context.GetDevice()->GetVKDevice(), m_ImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(_Context.GetDevice()->GetVKDevice(), m_RenderingCompleteSemaphores[i], nullptr);
        vkDestroyFence(_Context.GetDevice()->GetVKDevice(), m_InFlightFences[i], nullptr);
    }

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

    ASSERT(
        vkCreateDescriptorPool(_Context.GetDevice()->GetVKDevice(), &pool_info, nullptr, &imguiPool) == VK_SUCCESS,
        "Failed to initialize imgui pool");

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
    CommandBuffer::BeginRecording(cmdBuffers[m_CurrentFrame]);

    // Timer.
    timer += 7.0f * _DeltaTime;

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
        glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z),
                    glm::vec3(0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));

    // General data.
    glm::mat4 cameraView = _Camera->GetViewMatrix();
    glm::mat4 cameraProj = _Camera->GetProjectionMatrix();
    glm::vec4 cameraPos  = glm::vec4(_Camera->GetPosition(), 1.0f);
    glm::mat4 mat        = model->GetTransform();
    glm::mat4 mat2       = model2->GetTransform();

    // Update some of parts of the global UBO buffer
    globalParametersUBO.viewMatrix          = cameraView;
    globalParametersUBO.projMatrix          = cameraProj;
    globalParametersUBO.cameraPosition      = cameraPos;
    globalParametersUBO.dirLightPos         = directionalLightPosition;
    globalParametersUBO.directionalLightMVP = directionalLightMVP;
    globalParametersUBO.viewportDimension =
        glm::vec4(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, 0.0f, 0.0f);
    globalParametersUBO.DOFFramebufferSize.x = bokehPassFramebuffer->GetWidth();
    globalParametersUBO.DOFFramebufferSize.y = bokehPassFramebuffer->GetHeight();

    // Update point light positions. (Connected to the torch models.)
    globalParametersUBO.pointLightPositions[0] =
        glm::vec4(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[1] =
        glm::vec4(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[2] =
        glm::vec4(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f), 1.0f);
    globalParametersUBO.pointLightPositions[3] =
        glm::vec4(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f), 1.0f);

    // Update Particle system positions. (Connected to the torch models)
    fireSparks->SetEmitterPosition(
        glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f));
    fireSparks2->SetEmitterPosition(
        glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f));
    fireSparks3->SetEmitterPosition(
        glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f));
    fireSparks4->SetEmitterPosition(
        glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f));

    fireBase->SetEmitterPosition(
        glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.28f, torch1modelMatrix[3].z - 0.03f));
    fireBase2->SetEmitterPosition(
        glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.28f, torch2modelMatrix[3].z + 0.03f));
    fireBase3->SetEmitterPosition(
        glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.28f, torch3modelMatrix[3].z - 0.03f));
    fireBase4->SetEmitterPosition(
        glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.28f, torch4modelMatrix[3].z + 0.03f));

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

    if (globalParametersUBO.enablePointLightShadows.x == 1.0f)
    {
        // Shadow passes ---------
        glm::mat4 mat  = model->GetTransform();
        glm::mat4 mat2 = model2->GetTransform();

        // Start shadow pass.---------------------------------------------
        _ShadowMapRenderPass->Begin(cmdBuffers[m_CurrentFrame], *_DirectionalShadowMapFramebuffer);
        CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline);

        // Render the objects you want to cast shadows.
        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            shadowPassPipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &mat);
        model->DrawIndexed(cmdBuffers[m_CurrentFrame], shadowPassPipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            shadowPassPipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &mat2);
        model2->DrawIndexed(cmdBuffers[m_CurrentFrame], shadowPassPipeline->GetPipelineLayout());

        _ShadowMapRenderPass->End(cmdBuffers[m_CurrentFrame]);
        //   End shadow pass.---------------------------------------------

        // Start point shadow pass.--------------------
        for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
        {
            _PointShadowRenderPass->Begin(cmdBuffers[m_CurrentFrame], *_PointShadowMapFramebuffers[i]);
            CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowPassPipeline);

            glm::vec3 position = glm::vec3(
                globalParametersUBO.pointLightPositions[i].x,
                globalParametersUBO.pointLightPositions[i].y,
                globalParametersUBO.pointLightPositions[i].z);

            globalParametersUBO.shadowMatrices[i][0] = pointLightProjectionMatrix *
                glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][1] = pointLightProjectionMatrix *
                glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][2] =
                pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
            globalParametersUBO.shadowMatrices[i][3] = pointLightProjectionMatrix *
                glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
            globalParametersUBO.shadowMatrices[i][4] = pointLightProjectionMatrix *
                glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][5] = pointLightProjectionMatrix *
                glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));

            struct PC
            {
                glm::vec4 lightPos;
                glm::vec4 farPlane;
            };

            glm::vec4 pointLightIndex = glm::vec4(i);

            PC pc;
            pc.lightPos = glm::vec4(position, 1.0f);
            pc.farPlane = glm::vec4(pointFarPlane);

            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &mat);
            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_GEOMETRY_BIT,
                sizeof(glm::mat4),
                sizeof(glm::vec4),
                &pointLightIndex);
            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(glm::mat4) + sizeof(glm::vec4),
                sizeof(glm::vec4) + sizeof(glm::vec4),
                &pc);
            model->DrawIndexed(cmdBuffers[m_CurrentFrame], pointShadowPassPipeline->GetPipelineLayout());

            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT,
                0,
                sizeof(glm::mat4),
                &mat2);
            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_GEOMETRY_BIT,
                sizeof(glm::mat4),
                sizeof(glm::vec4),
                &pointLightIndex);
            CommandBuffer::PushConstants(
                cmdBuffers[m_CurrentFrame],
                pointShadowPassPipeline->GetPipelineLayout(),
                VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(glm::mat4) + sizeof(glm::vec4),
                sizeof(glm::vec4) + sizeof(glm::vec4),
                &pc);
            model2->DrawIndexed(cmdBuffers[m_CurrentFrame], pointShadowPassPipeline->GetPipelineLayout());

            _PointShadowRenderPass->End(cmdBuffers[m_CurrentFrame]);
            // CommandBuffer::EndRenderPass(cmdBuffers[m_CurrentFrame]);
            //   End point shadow pass.----------------------
        }
        // Shadow passes end  ----
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
    // Begin HDR rendering------------------------------------------
    // CommandBuffer::BeginRenderPass(cmdBuffers[m_CurrentFrame], HDRRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    _HDRRenderPass->Begin(cmdBuffers[m_CurrentFrame], *_HDRFramebuffer);
    //  Drawing the skybox.
    glm::mat4 skyBoxView = glm::mat4(glm::mat3(cameraView));
    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);

    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        skyboxPipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4),
        &skyBoxView);
    skybox->Draw(cmdBuffers[m_CurrentFrame], skyboxPipeline->GetPipelineLayout());

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
        glm::vec3(
            globalParametersUBO.pointLightPositions[4].x,
            globalParametersUBO.pointLightPositions[4].y,
            globalParametersUBO.pointLightPositions[4].z));
    lightCubeMat         = glm::scale(lightCubeMat, glm::vec3(0.05f));
    lightCubePC.modelMat = lightCubeMat;
    lightCubePC.color    = glm::vec4(4.5f, 1.0f, 1.0f, 1.0f);
    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);
    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        cubePipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4) + sizeof(glm::vec4),
        &lightCubePC);
    cube->Draw(cmdBuffers[m_CurrentFrame], cubePipeline->GetPipelineLayout());

    // Drawing the clouds.
    pushConst cloudsPC;
    glm::mat4 cloudsMat = glm::mat4(1.0f);
    glm::vec4 position  = glm::vec4(0, 10, 0, 0);
    glm::vec4 scale     = glm::vec4(0.5f, 0.5f, 0.5f, 0.5f);
    cloudsMat           = glm::translate(cloudsMat, glm::vec3(position.x, position.y, position.z));
    cloudsMat           = glm::scale(cloudsMat, glm::vec3(scale.x, scale.y, scale.z));
    cloudsPC.modelMat   = cloudsMat;
    cloudsPC.color      = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    // Drawing the Sponza.
    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);
    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
    model->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());

    // Drawing the helmet.
    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
    model2->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());

    // Drawing 4 torches.
    for (int i = 0; i < torch->GetMeshCount(); i++)
    {
        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            pipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &torch1modelMatrix);
        torch->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            pipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &torch2modelMatrix);
        torch->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            pipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &torch3modelMatrix);
        torch->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(
            cmdBuffers[m_CurrentFrame],
            pipeline->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &torch4modelMatrix);
        torch->DrawIndexed(cmdBuffers[m_CurrentFrame], pipeline->GetPipelineLayout());
    }

    pushConst swordPC;
    // Draw the emissive sword.
    swordPC.modelMat = model3->GetTransform();
    swordPC.color    = glm::vec4(0.1f, 3.0f, 0.1f, 1.0f);
    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);
    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        EmissiveObjectPipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_VERTEX_BIT,
        0,
        sizeof(glm::mat4) + sizeof(glm::vec4),
        &swordPC);
    model3->DrawIndexed(cmdBuffers[m_CurrentFrame], EmissiveObjectPipeline->GetPipelineLayout());

    // Draw the particles systems.
    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);

    glm::vec4 sparkBrigtness;
    sparkBrigtness.x = 5.0f;
    glm::vec4 flameBrigthness;
    flameBrigthness.x = 4.0f;
    glm::vec4 dustBrigthness;
    dustBrigthness.x = 1.0f;

    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        particleSystemPipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(glm::vec4),
        &sparkBrigtness);
    fireSparks->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireSparks2->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireSparks3->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireSparks4->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());

    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        particleSystemPipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(glm::vec4),
        &flameBrigthness);
    fireBase->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireBase2->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireBase3->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());
    fireBase4->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());

    CommandBuffer::PushConstants(
        cmdBuffers[m_CurrentFrame],
        particleSystemPipeline->GetPipelineLayout(),
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(glm::vec4),
        &dustBrigthness);
    ambientParticles->Draw(cmdBuffers[m_CurrentFrame], particleSystemPipeline->GetPipelineLayout());

    // vkCmdEndRenderingKHR(cmdBuffers[m_CurrentFrame]);

    _HDRRenderPass->End(cmdBuffers[m_CurrentFrame]);
    // CommandBuffer::EndRenderPass(cmdBuffers[m_CurrentFrame]);
    //   End HDR Rendering ------------------------------------------

    // Post processing begin ---------------------------

    bloomAgent->ApplyBloom(cmdBuffers[m_CurrentFrame]);

    if (enableDepthOfField)
    {
        //// Bokeh Pass start---------------------

        bokehRenderPass->Begin(cmdBuffers[m_CurrentFrame], *bokehPassFramebuffer);
        // CommandBuffer::BeginRenderPass(cmdBuffers[m_CurrentFrame], bokehPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, bokehPassPipeline);
        vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
        vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);

        vkCmdBindDescriptorSets(
            cmdBuffers[m_CurrentFrame],
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            bokehPassPipeline->GetPipelineLayout(),
            0,
            1,
            &bokehDescriptorSet,
            0,
            nullptr);
        vkCmdDraw(cmdBuffers[m_CurrentFrame], 3, 1, 0, 0);

        bokehRenderPass->End(cmdBuffers[m_CurrentFrame]);
        // CommandBuffer::EndRenderPass(cmdBuffers[m_CurrentFrame]);
    }
    // Bokeh pass end----------------------
    // Post processing end ---------------------------

    // ImGui::ShowDemoWindow();

    ImGui::Begin("Hello, world!"); // Create a window called "Hello, world!"
                                   // and append into it.

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
        pointLightShadows ? globalParametersUBO.enablePointLightShadows.x = 1.0f :
                            globalParametersUBO.enablePointLightShadows.x = 0.0f;
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

    // Start final scene render pass (to
    // swapchain).-------------------------------
    _SwapchainRenderPass->Begin(cmdBuffers[m_CurrentFrame], *_SwapchainFramebuffers[m_ActiveImageIndex]);

    CommandBuffer::BindPipeline(cmdBuffers[m_CurrentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline);
    vkCmdSetViewport(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicViewport);
    vkCmdSetScissor(cmdBuffers[m_CurrentFrame], 0, 1, &_DynamicScissor);
    vkCmdBindDescriptorSets(
        cmdBuffers[m_CurrentFrame],
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        finalPassPipeline->GetPipelineLayout(),
        0,
        1,
        &finalPassDescriptorSet,
        0,
        nullptr);
    vkCmdDraw(cmdBuffers[m_CurrentFrame], 3, 1, 0, 0);

    ImDrawData* draw_data = ImGui::GetDrawData();
    ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffers[m_CurrentFrame]);

    _SwapchainRenderPass->End(cmdBuffers[m_CurrentFrame]);
    //  End the command buffer recording
    //  phase(swapchain).-------------------------------.

    CommandBuffer::EndRecording(cmdBuffers[m_CurrentFrame]);
}

void ForwardRenderer::RenderImGui()
{
    // ImGui
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::Begin("Shaders:");
    bool breakFrame = false;
    for (const auto& entry : std::filesystem::directory_iterator((std::string(SOLUTION_DIR) + "/Engine/assets/shaders").c_str()))
    {
        int         fullstopIndex  = entry.path().string().find_last_of('.');
        int         lastSlashIndex = entry.path().string().find_last_of('\\');
        std::string path           = entry.path().string();
        std::string extension;
        std::string shadersFolder = std::string(SOLUTION_DIR) + "Engine/assets/shaders/";
        if (fullstopIndex != std::string::npos)
        {
            extension = path.substr(fullstopIndex, path.length() - fullstopIndex);
        }
        if (extension == ".frag" || extension == ".vert")
        {
            std::string shaderPath = path.substr(lastSlashIndex + 1, path.length() - lastSlashIndex);
            std::string shaderName = path.substr(lastSlashIndex + 1, fullstopIndex - (lastSlashIndex + 1));
            ImGui::PushID(shaderPath.c_str());
            if (ImGui::Button("Compile"))
            {
                std::string extensionWithoutDot = extension.substr(1, extension.length() - 1);
                for (int i = 0; i < extensionWithoutDot.length(); i++)
                {
                    extensionWithoutDot[i] = toupper(extensionWithoutDot[i]);
                }

                std::string outputName = shaderName + extensionWithoutDot + ".spv";
                std::string command    = std::string(SOLUTION_DIR) + "Engine/vendor/VULKAN/1.3.239.0/bin/glslc.exe " +
                    shadersFolder + shaderPath + " -o " + "shaders/" + outputName;
                int success = system(command.c_str());

                ImGui::PopID();
                ImGui::End();
                ImGui::EndFrame();
                breakFrame = true;
                break;
            }
            ImGui::PopID();
            ImGui::SameLine();
            ImGui::Text(shaderPath.c_str());
        }
    }

    if (breakFrame)
    {
        vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
        // WindowResize(); // TODO: THis crashes due to swapchain.
    }

    ImGui::End();
}

bool ForwardRenderer::BeginFrame()
{
    vkWaitForFences(_Context.GetDevice()->GetVKDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

    VkResult result;

    if (m_ActiveImageIndex == READY_TO_ACQUIRE)
    {
        result = vkAcquireNextImageKHR(
            _Context.GetDevice()->GetVKDevice(),
            _Swapchain->GetHandle(),
            UINT64_MAX,
            m_ImageAvailableSemaphores[m_CurrentFrame],
            VK_NULL_HANDLE,
            &m_ActiveImageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _Context.GetWindow()->IsWindowResized())
        {
            HandleWindowResize(result);
            return false;
        }

        ASSERT(result == VK_SUCCESS, "Failed to acquire next image.");
    }
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
        bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage);

        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);
        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassDepthSampler, nullptr);
        vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassSceneSampler, nullptr);

        finalPassSampler = Utils::CreateSampler(
            bokehPassImage,
            ImageType::COLOR,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_FALSE);
        Utils::UpdateDescriptorSet(
            finalPassDescriptorSet,
            finalPassSampler,
            bokehPassImage->GetImageView(),
            0,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bokehPassSceneSampler = Utils::CreateSampler(
            bloomAgent->GetPostProcessedImage(),
            ImageType::COLOR,
            VK_FILTER_LINEAR,
            VK_FILTER_LINEAR,
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            VK_FALSE);
        Utils::UpdateDescriptorSet(
            bokehDescriptorSet,
            bokehPassSceneSampler,
            bloomAgent->GetPostProcessedImage()->GetImageView(),
            0,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bokehPassDepthSampler = Utils::CreateSampler(
            HDRDepthImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::UpdateDescriptorSet(
            bokehDescriptorSet,
            bokehPassDepthSampler,
            HDRDepthImage->GetImageView(),
            1,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        // ~WindowResize()

        _Context.GetWindow()->OnResize();
        _Camera->SetViewportSize(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height);
        ImGui::EndFrame();
        m_ActiveImageIndex = READY_TO_ACQUIRE;
        CreateSynchronizationPrimitives();
    }
}

void ForwardRenderer::EndFrame()
{
    vkResetFences(_Context.GetDevice()->GetVKDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

    VkResult result;
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        _Camera->OnUpdate(_DeltaTime);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType                      = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore          waitSemaphores[] = { m_ImageAvailableSemaphores[m_CurrentFrame] };
    VkPipelineStageFlags waitStages[]     = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount         = 1;
    submitInfo.pWaitSemaphores            = waitSemaphores;
    submitInfo.pWaitDstStageMask          = waitStages;
    submitInfo.commandBufferCount         = 1;
    submitInfo.pCommandBuffers            = &cmdBuffers[m_CurrentFrame];
    VkSemaphore signalSemaphores[]        = { m_RenderingCompleteSemaphores[m_CurrentFrame] };
    submitInfo.signalSemaphoreCount       = 1;
    submitInfo.pSignalSemaphores          = signalSemaphores;

    VkQueue graphicsQueue                 = _Context.GetDevice()->GetGraphicsQueue();
    ASSERT(
        vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_InFlightFences[m_CurrentFrame]) == VK_SUCCESS,
        "Failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;
    VkSwapchainKHR swapChains[]    = { _Swapchain->GetHandle() };
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = swapChains;
    presentInfo.pImageIndices      = &m_ActiveImageIndex;
    presentInfo.pResults           = nullptr;

    result                         = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");

    m_CurrentFrame     = ++m_CurrentFrame % m_FramesInFlight;
    m_ActiveImageIndex = READY_TO_ACQUIRE;
}

void ForwardRenderer::PollEvents()
{
    glfwPollEvents();
}
