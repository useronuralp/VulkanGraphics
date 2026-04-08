#include "Bloom.h"
#include "Camera.h"
#include "CloudPass.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Framebuffer.h"
#include "Image.h"
#include "Instance.h"
#include "LightObject.h"
#include "Material.h"
#include "MeshBindingHelper.h"
#include "Model.h"
#include "ParticleSystem.h"
#include "PhysicalDevice.h"
#include "Pipeline.h"
#include "Renderer.h"
#include "Renderer/RenderPass.h"
#include "Scene.h"
#include "StaticMeshObject.h"
#include "Surface.h"
#include "Swapchain.h"
#include "Utils.h"
#include "VulkanContext.h"
#include "Window.h"

#include <Curl.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define POINT_LIGHT_COUNT 5 // todo: dynamically calculate this from actual point light number registered to the scene.

static bool printedDependencies = false;

extern FrameSync GFrameSync;

// VulkanRenderer.cpp
VulkanRenderer::VulkanRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
    : _Context(InContext), _Swapchain(InSwapchain), _Camera(InCamera)
{
}

bool VulkanRenderer::BeginFrame()
{
    auto device = _Context.GetDevice()->GetVKDevice();

    vkWaitForFences(device, 1, &GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex], VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex]);

    VkResult result = vkAcquireNextImageKHR(
        device, _Swapchain->GetHandle(), UINT64_MAX, GFrameSync.AcquireFinishedSemaphores[GFrameSync.CurrentBufferIndex], VK_NULL_HANDLE, &_CurrentSwapchainImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _Context.GetWindow()->IsWindowResized())
    {
        HandleWindowResize(result);
        return false;
    }

    ENSURE(result == VK_SUCCESS, "Failed to acquire next image.");
    return true;
}

void VulkanRenderer::EndFrame()
{
    if (!ImGui::GetIO().WantCaptureMouse)
    {
        _Camera->OnUpdate(_DeltaTime);
    }

    auto queue                        = _Context.GetDevice()->GetGraphicsQueue();

    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo         submitInfo{};

    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &GFrameSync.AcquireFinishedSemaphores[GFrameSync.CurrentBufferIndex];
    submitInfo.pWaitDstStageMask    = waitStages;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &_CmdBuffers[GFrameSync.CurrentBufferIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &GFrameSync.RenderingCompleteSemaphores[_CurrentSwapchainImageIndex];

    ENSURE(vkQueueSubmit(queue, 1, &submitInfo, GFrameSync.InFlightFences[GFrameSync.CurrentBufferIndex]) == VK_SUCCESS, "Failed to submit draw command buffer!");

    VkSwapchainKHR   swapchain = _Swapchain->GetHandle();
    VkPresentInfoKHR presentInfo{};

    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &GFrameSync.RenderingCompleteSemaphores[_CurrentSwapchainImageIndex];
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapchain;
    presentInfo.pImageIndices      = &_CurrentSwapchainImageIndex;
    presentInfo.pResults           = nullptr;

    VkResult result                = vkQueuePresentKHR(queue, &presentInfo);
    ENSURE(result == VK_SUCCESS, "Failed to present swap chain image!");
}

void VulkanRenderer::InitImGui()
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

    ENSURE(vkCreateDescriptorPool(_Context.GetDevice()->GetVKDevice(), &pool_info, nullptr, &_ImGuiPool) == VK_SUCCESS, "Failed to initialize imgui pool");

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(_Context.GetWindow()->GetNativeWindow(), true);

    _ImGuiInitInfo.Instance       = _Context.GetInstance()->GetVkInstance();
    _ImGuiInitInfo.PhysicalDevice = _Context.GetPhysicalDevice()->GetVKPhysicalDevice();
    _ImGuiInitInfo.Device         = _Context.GetDevice()->GetVKDevice();
    _ImGuiInitInfo.Queue          = _Context.GetDevice()->GetGraphicsQueue();
    _ImGuiInitInfo.DescriptorPool = _ImGuiPool;
    _ImGuiInitInfo.MinImageCount  = 3;
    _ImGuiInitInfo.ImageCount     = 3;
    _ImGuiInitInfo.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&_ImGuiInitInfo, _SwapchainRenderPass->GetHandle());

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF((std::string(SOLUTION_DIR) + "Engine/assets/resources/Open_Sans/static/OpenSans/OpenSans-Regular.ttf").c_str(), 30);

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

void VulkanRenderer::RenderImGui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void VulkanRenderer::Cleanup()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        CommandBuffer::FreeCommandBuffer(_CmdBuffers[i], _CmdPool, _Context.GetDevice()->GetGraphicsQueue());
    }
    CommandBuffer::DestroyCommandPool(_CmdPool);

    ImGui_ImplVulkan_DestroyFontUploadObjects();
    vkDestroyDescriptorPool(_Context.GetDevice()->GetVKDevice(), _ImGuiPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
}

void VulkanRenderer::SetScene(const Ref<Scene>& InScene)
{
    _Scene = InScene;
}

Ref<Scene> VulkanRenderer::GetScene() const
{
    return _Scene;
}

void VulkanRenderer::UpdateViewport_Scissor()
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

void VulkanRenderer::CreateSwapchainRenderPass()
{
    RenderPass::AttachmentInfo colorAttachment{ _Context.GetSurface()->GetVKSurfaceFormat().format,
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                                VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                { 0.0f, 0.0f, 0.0f, 0.0f } };

    VkSubpassDependency dep{};
    dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass    = 0;
    dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    RenderPass::CreateInfo createInfo{ { colorAttachment }, { dep }, false, "Swapchain Render Pass" };

    _SwapchainRenderPass = std::make_unique<RenderPass>(_Context, createInfo);
}

void VulkanRenderer::CreateSwapchainFramebuffers()
{
    _SwapchainFramebuffers.clear();

    for (auto imageView : _Swapchain->GetImageViews())
    {
        std::vector<VkImageView> attachments = { imageView };
        _SwapchainFramebuffers.push_back(
            make_s<Framebuffer>(_SwapchainRenderPass->GetHandle(), attachments, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height));
    }
}

void VulkanRenderer::HandleWindowResize(VkResult InResult)
{
    if (InResult == VK_ERROR_OUT_OF_DATE_KHR || _Context.GetWindow()->IsWindowResized() || InResult == VK_SUBOPTIMAL_KHR)
    {
        vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

        int width = 0, height = 0;
        glfwGetFramebufferSize(_Context.GetWindow()->GetNativeWindow(), &width, &height);
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(_Context.GetWindow()->GetNativeWindow(), &width, &height);
            glfwWaitEvents();
        }

        _Swapchain->Recreate();

        UpdateViewport_Scissor();
        CreateSwapchainFramebuffers();

        // Subclass handles its own resize (HDR framebuffers, bloom, bokeh, etc.)
        OnResize();

        _Context.GetWindow()->OnResize();
        _Camera->SetViewportSize(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height);
        ImGui::EndFrame();
    }
}

VulkanForwardRenderer::VulkanForwardRenderer(VulkanContext& InContext, Ref<Swapchain> InSwapchain, Ref<Camera> InCamera)
    : VulkanRenderer(InContext, InSwapchain, InCamera)
{
}

void VulkanForwardRenderer::OnResize()
{
    CreateHDRFramebuffer();
    CreateBokehFramebuffer();
    SetupBokehPassPipeline();

    bloomAgent = make_s<Bloom>();
    bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage, _CloudPass);

    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassDepthSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassSceneSampler, nullptr);

    finalPassSampler = Utils::CreateSampler(bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassSceneSampler =
        Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        bokehDescriptorSet, bokehPassSceneSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bokehPassDepthSampler = Utils::CreateSampler(HDRDepthImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(bokehDescriptorSet, bokehPassDepthSampler, HDRDepthImage->GetImageView(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}

Ref<Model> VulkanForwardRenderer::LoadPBRModel(const std::string& InPath, LoadingFlags InFlags)
{
    auto model = make_s<Model>(InPath, InFlags);
    MeshBinding::BindModelPBR(model, pool, PBRLayout, globalParametersUBOBuffer, sizeof(GlobalParametersUBO), directionalShadowMapImage, pointShadowMaps);
    return model;
}

Ref<Model> VulkanForwardRenderer::LoadSimpleModel(const std::string& InPath, LoadingFlags InFlags)
{
    auto model = make_s<Model>(InPath, InFlags);
    MeshBinding::BindModelSimple(model, pool, emissiveLayout, globalParametersUBOBuffer, sizeof(glm::mat4) * 2);
    return model;
}

Ref<Model> VulkanForwardRenderer::LoadSkyboxModel(const float* InVertices, uint32_t InVertexCount, Ref<Image> InCubemap)
{
    auto model = make_s<Model>(InVertices, InVertexCount, InCubemap);
    MeshBinding::BindSkybox(model->GetMeshes()[0], pool, skyboxLayout, globalParametersUBOBuffer);
    return model;
}

Ref<Model> VulkanForwardRenderer::LoadDebugModel(const float* InVertices, uint32_t InVertexCount)
{
    auto model = make_s<Model>(InVertices, InVertexCount);
    MeshBinding::BindSimple(model->GetMeshes()[0], pool, cubeLayout, globalParametersUBOBuffer, sizeof(glm::mat4) * 2);
    return model;
}

Ref<Material> VulkanForwardRenderer::GetPBRMaterial() const
{
    return pbrMaterial;
}

Ref<Material> VulkanForwardRenderer::GetEmissiveMaterial() const
{
    return emissiveMaterial;
}

Ref<Material> VulkanForwardRenderer::GetSkyboxMaterial() const
{
    return skyboxMaterial;
}

Ref<Material> VulkanForwardRenderer::GetCubeMaterial() const
{
    return cubeMaterial;
}

Ref<ParticleSystem> VulkanForwardRenderer::CreateParticleSystem(const ParticleSpecs& InSpecs, const Ref<Image>& InTexture)
{
    auto ps = make_s<ParticleSystem>(InSpecs, InTexture, particleSystemLayout, pool);
    ps->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    return ps;
}

void VulkanForwardRenderer::Cleanup()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());

    // Forward-renderer-specific cleanup
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassSceneSampler, nullptr);
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), bokehPassDepthSampler, nullptr);
    vkFreeMemory(_Context.GetDevice()->GetVKDevice(), globalParametersUBOBufferMemory, nullptr);
    vkDestroyBuffer(_Context.GetDevice()->GetVKDevice(), globalParametersUBOBuffer, nullptr);

    // Base class cleanup (command buffers, ImGui)
    VulkanRenderer::Cleanup();
}

void VulkanForwardRenderer::Init()
{
    _Graph    = make_u<RenderGraph>(_Context);
    startTime = std::chrono::high_resolution_clock::now();

    UpdateViewport_Scissor();

    pool = make_s<DescriptorPool>(
        200,
        std::vector<VkDescriptorType>{
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER });

    auto extent = _Context.GetSurface()->GetVKExtent();
    auto device = _Context.GetDevice()->GetVKDevice();
    _CloudPass  = make_s<CloudPass>();
    _CloudPass->Create(device, extent, pool->GetDescriptorPool());

    CurlNoise::SetCurlSettings(false, 4.0f, 6, 1.0, 0.0);
    pointShadowMaps.resize(POINT_LIGHT_COUNT);
    _PointShadowMapFramebuffers.resize(POINT_LIGHT_COUNT);

    std::vector<DescriptorBinding> hdrLayout{
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT },
        { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5, VK_SHADER_STAGE_FRAGMENT_BIT },
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

    particleSystemLayout = make_s<DescriptorSetLayout>(ParticleSystemLayout);
    skyboxLayout         = make_s<DescriptorSetLayout>(SkyboxLayout);
    cubeLayout           = make_s<DescriptorSetLayout>(CubeLayout);
    PBRLayout            = make_s<DescriptorSetLayout>(hdrLayout);
    swapchainLayout      = make_s<DescriptorSetLayout>(SwapchainLayout);
    emissiveLayout       = make_s<DescriptorSetLayout>(EmissiveLayout);
    bokehPassLayout      = make_s<DescriptorSetLayout>(BokehPassLayout);

    Utils::CreateVKBuffer(
        sizeof(GlobalParametersUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        globalParametersUBOBuffer,
        globalParametersUBOBufferMemory);
    vkMapMemory(device, globalParametersUBOBufferMemory, 0, sizeof(GlobalParametersUBO), 0, &mappedGlobalParametersModelUBOBuffer);

    directionalShadowMapImage =
        make_s<Image>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), ImageType::DEPTH);

    for (int i = 0; i < POINT_LIGHT_COUNT; i++)
    {
        pointShadowMaps[i] = make_s<Image>(
            POUNT_SHADOW_DIM,
            POUNT_SHADOW_DIM,
            VK_FORMAT_D32_SFLOAT,
            (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
            ImageType::DEPTH_CUBEMAP);
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &swapchainLayout->GetDescriptorLayout();

    VkResult rslt                = vkAllocateDescriptorSets(device, &allocInfo, &finalPassDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    allocInfo.pSetLayouts = &bokehPassLayout->GetDescriptorLayout();
    rslt                  = vkAllocateDescriptorSets(device, &allocInfo, &bokehDescriptorSet);
    ENSURE(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

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

    pbrMaterial                          = make_s<Material>("PBR", pipeline, PBRLayout);
    emissiveMaterial                     = make_s<Material>("Emissive", EmissiveObjectPipeline, emissiveLayout);
    skyboxMaterial                       = make_s<Material>("Skybox", skyboxPipeline, skyboxLayout);
    cubeMaterial                         = make_s<Material>("Cube", cubePipeline, cubeLayout);

    std::vector<VkImageView> attachments = { directionalShadowMapImage->GetImageView() };
    _DirectionalShadowMapFramebuffer     = make_s<Framebuffer>(_ShadowMapRenderPass->GetHandle(), attachments, SHADOW_DIM, SHADOW_DIM);

    // Framebuffers need for point light shadows. (Dependent on the number
    // of point lights in the scene)
    for (int i = 0; i < POINT_LIGHT_COUNT; i++)
    {
        attachments                    = { pointShadowMaps[i]->GetImageView() };

        _PointShadowMapFramebuffers[i] = make_s<Framebuffer>(_PointShadowRenderPass->GetHandle(), attachments, POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, 6);
    }

    // UBO constants
    globalParametersUBO.cameraNearPlane = glm::vec4(_Camera->GetNearClip());
    globalParametersUBO.cameraFarPlane  = glm::vec4(_Camera->GetFarClip());
    globalParametersUBO.focalDepth      = glm::vec4(1.5f);
    globalParametersUBO.focalLength     = glm::vec4(15.0f);
    globalParametersUBO.fstop           = glm::vec4(6.0f);
    globalParametersUBO.pointFarPlane   = glm::vec4(pointFarPlane);
    globalParametersUBO.pointLightCount = glm::vec4(POINT_LIGHT_COUNT);

    // ── Command Buffers ─────────────────────────────────────

    CommandBuffer::CreateCommandBufferPool(_Context._QueueFamilies.GraphicsFamily, _CmdPool);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        CommandBuffer::CreateCommandBuffer(_CmdBuffers[i], _CmdPool);

    // ── Post Processing ─────────────────────────────────────

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

void VulkanForwardRenderer::SetupPBRPipeline()
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

void VulkanForwardRenderer::SetupFinalPassPipeline()
{
    finalPassPipeline = PipelineBuilder(_Context)
                            .SetRenderPass(_SwapchainRenderPass->GetHandle())
                            .SetDescriptorSetLayout(swapchainLayout)
                            .SetVertexShader("assets/shaders/quadRenderVERT.spv")
                            .SetFragmentShader("assets/shaders/swapchainFRAG.spv")
                            .Build();
}
void VulkanForwardRenderer::SetupShadowPassPipeline()
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
void VulkanForwardRenderer::SetupPointShadowPassPipeline()
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
void VulkanForwardRenderer::SetupSkyboxPipeline()
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
void VulkanForwardRenderer::SetupCubePipeline()
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

void VulkanForwardRenderer::SetupParticleSystemPipeline()
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
void VulkanForwardRenderer::SetupEmissiveObjectPipeline()
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

void VulkanForwardRenderer::CreateHDRRenderPass()
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
void VulkanForwardRenderer::CreateShadowRenderPass()
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

void VulkanForwardRenderer::CreatePointShadowRenderPass()
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

void VulkanForwardRenderer::EnableDepthOfField()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);

    finalPassSampler = Utils::CreateSampler(bokehPassImage, ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(finalPassDescriptorSet, finalPassSampler, bokehPassImage->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}
// Connects the bloom image to the final render pass.
void VulkanForwardRenderer::DisableDepthOfField()
{
    vkDeviceWaitIdle(_Context.GetDevice()->GetVKDevice());
    vkDestroySampler(_Context.GetDevice()->GetVKDevice(), finalPassSampler, nullptr);

    finalPassSampler =
        Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
    Utils::UpdateDescriptorSet(
        finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void VulkanForwardRenderer::SetupBokehPassPipeline()
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
void VulkanForwardRenderer::CreateBokehRenderPass()
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

void VulkanForwardRenderer::CreateHDRFramebuffer()
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

void VulkanForwardRenderer::CreateBokehFramebuffer()
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

void VulkanForwardRenderer::SyncLightsToUBO()
{
    auto dirLight                                 = _Scene->GetDirectionalLight();
    globalParametersUBO.dirLightPos               = glm::vec4(dirLight->GetPosition(), 1.0f);
    globalParametersUBO.directionalLightIntensity = glm::vec4(dirLight->GetIntensity());

    auto& pointLights                             = _Scene->GetPointLights();
    int   pointIndex                              = 0;

    for (int i = 0; i < pointLights.size() && pointIndex < MAX_POINT_LIGHT_COUNT; i++, pointIndex++)
    {
        globalParametersUBO.pointLightPositions[pointIndex]   = glm::vec4(pointLights[i]->GetPosition(), 1.0f);
        globalParametersUBO.pointLightColors[pointIndex]      = glm::vec4(pointLights[i]->GetColor(), 1.0f);
        globalParametersUBO.pointLightIntensities[pointIndex] = glm::vec4(pointLights[i]->GetIntensity());
    }

    globalParametersUBO.pointLightCount = glm::vec4(POINT_LIGHT_COUNT);
}

void VulkanForwardRenderer::RenderFrame(const float InDeltaTime)
{
    if (!ENSURE_CHECK(_Scene, "Scene wasn't valid, create a scene first to start rendering."))
    {
        return;
    }

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

        auto torchGroups = _Scene->GetTorchGroups();
        for (int i = 0; i < torchGroups.size(); i++)
        {
            torchGroups[i]->Light->SetIntensity(distributions[i](gen));
        }
    }

    SyncLightsToUBO();

    // Update scene (particles)
    _Scene->Update(_DeltaTime);

    CommandBuffer::BeginRecording(_CmdBuffers[GFrameSync.CurrentBufferIndex]);

    timer += 7.0f * _DeltaTime;

    auto  now  = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float>(now - startTime).count();

    // Rotate the helmet
    auto helmet = _Scene->FindMeshObject("Helmet");
    if (helmet)
        helmet->Rotate(2.0f * _DeltaTime, glm::vec3(0, 1, 0));

    // Directional light MVP
    auto      dirLight            = _Scene->GetDirectionalLight();
    glm::mat4 directionalLightMVP = directionalLightProjectionMatrix * glm::lookAt(dirLight->GetPosition(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Camera data
    glm::mat4 cameraView = _Camera->GetViewMatrix();
    glm::mat4 cameraProj = _Camera->GetProjectionMatrix();
    glm::vec4 cameraPos  = glm::vec4(_Camera->GetPosition(), 1.0f);

    // UBO updates
    globalParametersUBO.viewMatrix           = cameraView;
    globalParametersUBO.projMatrix           = cameraProj;
    globalParametersUBO.cameraPosition       = cameraPos;
    globalParametersUBO.dirLightPos          = glm::vec4(dirLight->GetPosition(), 1.0f);
    globalParametersUBO.directionalLightMVP  = directionalLightMVP;
    globalParametersUBO.viewportDimension    = glm::vec4(_Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, 0.0f, 0.0f);
    globalParametersUBO.DOFFramebufferSize.x = bokehPassFramebuffer->GetWidth();
    globalParametersUBO.DOFFramebufferSize.y = bokehPassFramebuffer->GetHeight();

    auto shadowCasters                       = _Scene->GetShadowCasters();
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

                    for (const auto caster : shadowCasters)
                    {
                        glm::mat4 transform = caster->GetTransform();
                        CommandBuffer::PushConstants(cmd, shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
                        caster->GetModel()->DrawIndexed(cmd, shadowPassPipeline->GetPipelineLayout());
                    }

                    _ShadowMapRenderPass->End(cmd);
                };
            });
        // ~Directional Shadow Pass ////////////////////////////////////// RDG

        for (int i = 0; i < POINT_LIGHT_COUNT; i++)
        {
            auto* res = _Graph->CreateTexture(
                "PointShadow_" + std::to_string(i), pointShadowMaps[i]->GetVKImage(), VK_FORMAT_D32_SFLOAT, POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, false);
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
                    for (int i = 0; i < POINT_LIGHT_COUNT; i++)
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

                        glm::vec4 pointLightIndex = glm::vec4(i);

                        struct PC
                        {
                            glm::vec4 lightPos;
                            glm::vec4 farPlane;
                        };
                        PC pc;
                        pc.lightPos = glm::vec4(position, 1.0f);
                        pc.farPlane = glm::vec4(pointFarPlane);

                        for (const auto caster : shadowCasters)
                        {
                            glm::mat4 transform = caster->GetTransform();
                            CommandBuffer::PushConstants(cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
                            CommandBuffer::PushConstants(
                                cmd, pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &pointLightIndex);
                            CommandBuffer::PushConstants(
                                cmd,
                                pointShadowPassPipeline->GetPipelineLayout(),
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                                sizeof(glm::mat4) + sizeof(glm::vec4),
                                sizeof(glm::vec4) * 2,
                                &pc);
                            caster->GetModel()->DrawIndexed(cmd, pointShadowPassPipeline->GetPipelineLayout());
                        }

                        _PointShadowRenderPass->End(cmd);
                    }
                };
            });
        // ~Point Shadow Pass ////////////////////////////////////// RDG
    }

    memcpy(mappedGlobalParametersModelUBOBuffer, &globalParametersUBO, sizeof(GlobalParametersUBO));

    // Sprite animation
    aniamtionRate -= _DeltaTime;
    if (aniamtionRate <= 0)
    {
        aniamtionRate = 0.01388888f;
        currentAnimationFrame++;
        if (currentAnimationFrame > 72)
            currentAnimationFrame = 0;
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
                for (auto group : _Scene->GetTorchGroups())
                {
                    group->Flame->RowOffset    = 0.0833333333333333333333f * j;
                    group->Flame->ColumnOffset = 0.166666666666666f * i;
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
        true);

    auto SceneDepth = _Graph->CreateTexture(
        "SceneDepth", HDRDepthImage->GetVKImage(), VK_FORMAT_D32_SFLOAT, _Context.GetSurface()->GetVKExtent().width, _Context.GetSurface()->GetVKExtent().height, false);

    // HDR Pass ////////////////////////////////////// RDG
    _Graph->AddPass(
        "HDR Scene",
        [&](RGPass& pass)
        {
            pass.Reads = { dirShadowMap };
            pass.Reads.insert(pass.Reads.end(), pointShadowArray.begin(), pointShadowArray.end());
            pass.Writes     = { SceneColorHDR, SceneDepth };

            pass.RecordFunc = [&](VkCommandBuffer cmd)
            {
                _HDRRenderPass->Begin(cmd, *_HDRFramebuffer, "HDR Pass");

                // Skybox
                auto skybox = _Scene->GetSkybox();
                if (skybox)
                {
                    glm::mat4 skyBoxView = glm::mat4(glm::mat3(cameraView));
                    skybox->GetMaterial()->Bind(cmd);
                    vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                    vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                    CommandBuffer::PushConstants(cmd, skybox->GetMaterial()->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
                    skybox->GetModel()->Draw(cmd, skybox->GetMaterial()->GetPipelineLayout());
                }

                // Debug objects (light cube)
                for (auto obj : _Scene->GetDebugObjects())
                {
                    // Follow the red light position
                    if (obj->GetName() == "Light Cube" && _Scene->GetPointLightCount() > 0)
                    {
                        auto& pointLights = _Scene->GetPointLights();
                        for (auto& light : pointLights)
                        {
                            if (light->GetName() == "Red Light")
                            {
                                obj->SetPosition(light->GetPosition());
                                break;
                            }
                        }
                    }

                    struct
                    {
                        glm::mat4 modelMat;
                        glm::vec4 color;
                    } pc;
                    pc.modelMat = obj->GetTransform();
                    pc.color    = glm::vec4(4.5f, 1.0f, 1.0f, 1.0f);
                    obj->GetMaterial()->Bind(cmd);
                    vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                    vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                    CommandBuffer::PushConstants(cmd, obj->GetMaterial()->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &pc);
                    obj->GetModel()->Draw(cmd, obj->GetMaterial()->GetPipelineLayout());
                }

                // PBR mesh objects
                Ref<Material> currentMaterial = nullptr;
                for (auto obj : _Scene->GetMeshObjects())
                {
                    if (obj->GetMaterial() != currentMaterial)
                    {
                        currentMaterial = obj->GetMaterial();
                        currentMaterial->Bind(cmd);
                        vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                        vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                    }
                    glm::mat4 transform = obj->GetTransform();
                    CommandBuffer::PushConstants(cmd, currentMaterial->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &transform);
                    obj->GetModel()->DrawIndexed(cmd, currentMaterial->GetPipelineLayout());
                }

                // Emissive objects
                for (auto obj : _Scene->GetEmissiveObjects())
                {
                    struct
                    {
                        glm::mat4 modelMat;
                        glm::vec4 color;
                    } pc;
                    pc.modelMat = obj->GetTransform();
                    pc.color    = glm::vec4(0.1f, 3.0f, 0.1f, 1.0f);
                    obj->GetMaterial()->Bind(cmd);
                    vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                    vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);
                    CommandBuffer::PushConstants(cmd, obj->GetMaterial()->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &pc);
                    obj->GetModel()->DrawIndexed(cmd, obj->GetMaterial()->GetPipelineLayout());
                }

                // Particles
                CommandBuffer::BindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline);
                vkCmdSetViewport(cmd, 0, 1, &_DynamicViewport);
                vkCmdSetScissor(cmd, 0, 1, &_DynamicScissor);

                glm::vec4 sparkBrightness = glm::vec4(5.0f, 0.0f, 0.0f, 0.0f);
                glm::vec4 flameBrightness = glm::vec4(4.0f, 0.0f, 0.0f, 0.0f);
                glm::vec4 dustBrightness  = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &sparkBrightness);
                for (auto& group : _Scene->GetTorchGroups())
                    group->Sparks->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &flameBrightness);
                for (auto& group : _Scene->GetTorchGroups())
                    group->Flame->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

                CommandBuffer::PushConstants(cmd, particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &dustBrightness);
                if (_Scene->GetAmbientParticles())
                    _Scene->GetAmbientParticles()->Draw(cmd, particleSystemPipeline->GetPipelineLayout());

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

    // Bokeh Pass ////////////////////////////////////// RDG
    if (enableDepthOfField)
    {
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
    }
    // ~Bokeh Pass ////////////////////////////////////// RDG

    auto swapchainOutput = _Graph->CreateTexture(
        "Swapchain Image",
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

                // ImGui
                ImGui::Begin("Hello, world!");

                glm::vec3 dirPos = _Scene->GetDirectionalLight()->GetPosition();
                if (ImGui::DragFloat3("Directional Light", &dirPos.x, 0.1f, -50, 50))
                    _Scene->GetDirectionalLight()->SetPosition(dirPos);

                auto helmetObj = _Scene->FindMeshObject("Helmet");
                if (helmetObj)
                {
                    glm::vec3 helmetPos = helmetObj->GetPosition();
                    if (ImGui::DragFloat3("Helmet", &helmetPos.x, 0.01f, -10, 10))
                        helmetObj->SetPosition(helmetPos);
                }

                for (auto emissive : _Scene->GetEmissiveObjects())
                {
                    glm::vec3 pos = emissive->GetPosition();
                    if (ImGui::DragFloat3(emissive->GetName().c_str(), &pos.x, 0.01f, -10, 10))
                        emissive->SetPosition(pos);
                }

                for (int i = 0; i < _Scene->GetTorchGroups().size(); i++)
                {
                    auto      group = _Scene->GetTorchGroups()[i];
                    glm::vec3 pos   = group->Torch->GetPosition();
                    if (ImGui::DragFloat3(("Torch " + std::to_string(i + 1)).c_str(), &pos.x, 0.01f, -10, 10))
                    {
                        group->Torch->SetPosition(pos);
                        group->Light->SetPosition(pos + glm::vec3(0.0f, 0.22f, 0.0f));
                        group->Sparks->SetEmitterPosition(pos + glm::vec3(0.0f, 0.22f, 0.0f));
                        group->Flame->SetEmitterPosition(pos + glm::vec3(0.0f, 0.28f, 0.0f));
                    }
                }

                for (auto light : _Scene->GetPointLights())
                {
                    if (light->GetName() == "Red Light")
                    {
                        glm::vec3 redPos = light->GetPosition();
                        if (ImGui::DragFloat3("Red Light", &redPos.x, 0.01f, -10, 10))
                            light->SetPosition(redPos);
                    }
                }

                if (ImGui::Checkbox("Point light shadows", &pointLightShadows))
                    pointLightShadows ? globalParametersUBO.enablePointLightShadows.x = 1.0f : globalParametersUBO.enablePointLightShadows.x = 0.0f;

                if (ImGui::Checkbox("Enable Depth of Field", &enableDepthOfField))
                    enableDepthOfField ? EnableDepthOfField() : DisableDepthOfField();

                if (ImGui::Checkbox("Show DOF focus", &showDOFFocus))
                    showDOFFocus ? globalParametersUBO.showDOFFocus.x = 1.0f : globalParametersUBO.showDOFFocus.x = 0.0f;

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

    _Graph->Execute(_CmdBuffers[GFrameSync.CurrentBufferIndex]);
    CommandBuffer::EndRecording(_CmdBuffers[GFrameSync.CurrentBufferIndex]);
    _Graph->Clear();
}