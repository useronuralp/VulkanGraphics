#include "VulkanApplication.h"
#include "Model.h"
#include "Image.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "Surface.h"
using namespace Anor;
class MyApplication : public Anor::VulkanApplication
{
    const uint32_t SHADOW_DIM = 10000;
public:
    Model* model;
    Model* model2;
    Model* lightSphere;
    Mesh* skybox;
    Mesh* debugQuad;
    float timer = 0.0f;
    glm::vec3 pointLightPosition = glm::vec3(50.0f, 50.0f, 50.0f);
    glm::vec3 directionalLightPosition = glm::vec3(35.0f, 35.0f, 9.0f);
    float near_plane = 5.0f;
    float far_plane = 200.0f;


    glm::mat4 lightView = glm::lookAt(directionalLightPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, near_plane, far_plane);
    glm::mat4 lightModel = glm::mat4(1.0f);
    glm::mat4 depthMVP;


    float rendertime;
    float m_LastFrameRenderTime;
    Ref<Texture> shadowMapTexture;
    Ref<Image> shadowMapImage;

    Ref<RenderPass> shadowMapPass;
    Ref<Framebuffer> shadowMapFramebuffer;


    void SetupScenePassConfiguration(Model* model)
    {
        std::vector<DescriptorLayout> dscLayout 
        {
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 0
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 1
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 2
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 3
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::VEC3,                       ShaderStage::VERTEX   }, // Index 4
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::VEC3,                       ShaderStage::FRAGMENT }, // Index 5
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::VEC3,                       ShaderStage::FRAGMENT }, // Index 6
            DescriptorLayout { Type::TEXTURE_SAMPLER, Size::DIFFUSE_SAMPLER,            ShaderStage::FRAGMENT }, // Index 7
            DescriptorLayout { Type::TEXTURE_SAMPLER, Size::NORMAL_SAMPLER,             ShaderStage::FRAGMENT }, // Index 8
            DescriptorLayout { Type::TEXTURE_SAMPLER, Size::ROUGHNESS_METALLIC_SAMPLER, ShaderStage::FRAGMENT }, // Index 9
            DescriptorLayout { Type::TEXTURE_SAMPLER, Size::SHADOWMAP_SAMPLER,          ShaderStage::FRAGMENT }, // Index 10
        };


        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE; 
        specs.RenderPass = s_ModelRenderPass;
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableBlending = VK_TRUE;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/PBRShaderVERT.spv";
        specs.FragmentShaderPath = "shaders/PBRShaderFRAG.spv";


        model->AddConfiguration("NormalRenderPass", specs, dscLayout);

    }
    void SetupShadowPassConfiguration(Model* model)
    {
        std::vector<DescriptorLayout> dscLayout
        {
            DescriptorLayout { Type::UNIFORM_BUFFER, Size::MAT4, ShaderStage::VERTEX },  // Index 0
            DescriptorLayout { Type::UNIFORM_BUFFER, Size::MAT4, ShaderStage::VERTEX },  // Index 0
        };


        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE;
        specs.RenderPass = shadowMapPass;
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 1.25f;
        specs.DepthBiasSlopeFactor = 1.75f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableBlending = VK_FALSE;
        specs.EnableDepthBias = VK_TRUE;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/shadowPassVERT.spv";
        specs.FragmentShaderPath = "shaders/shadowPassFRAG.spv";
        specs.ViewportHeight = SHADOW_DIM;
        specs.ViewportWidth = SHADOW_DIM;

        model->AddConfiguration("ShadowMapPass", specs, dscLayout);
    }

    void SetupLightSphere(Model* model)
    {
        std::vector<DescriptorLayout> dscLayout
        { 
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX },
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX },
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX },
        };

        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE;
        specs.RenderPass = s_ModelRenderPass;
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS;
        specs.EnableBlending = VK_TRUE;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/lightsourceVERT.spv";
        specs.FragmentShaderPath = "shaders/lightsourceFRAG.spv";

        model->AddConfiguration("NormalRenderPass", specs, dscLayout);
    }
    void SetupSkybox(Mesh* mesh)
    {
        std::vector<DescriptorLayout> dscLayout =
        {
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::TEXTURE_SAMPLER,Size::CUBEMAP_SAMPLER,ShaderStage::FRAGMENT}
        };

        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE;
        specs.RenderPass = s_ModelRenderPass;
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableBlending = VK_FALSE;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_FALSE;
        specs.EnableDepthWriting = VK_FALSE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/cubemapVERT.spv";
        specs.FragmentShaderPath = "shaders/cubemapFRAG.spv";

        mesh->AddConfiguration("NormalRenderPass", specs, dscLayout);
    }

private:
    void OnStart()
    {
        // Setup the image for shadow map rendering.
        shadowMapImage = std::make_shared<Image>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, ImageType::DEPTH);
        // Setup the texture that we will use to sample from in our actual scene render pass.
        shadowMapTexture = std::make_shared<Texture>(shadowMapImage);

        // Create a depth render pass here that we will pass to the engine.
        VkAttachmentDescription depthAttachmentDescription;
        VkAttachmentReference depthAttachmentRef;
        
        depthAttachmentDescription.format = VK_FORMAT_D32_SFLOAT;
        depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDescription.flags = 0;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        
        depthAttachmentRef.attachment = 0;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 0;
        subpass.pColorAttachments = 0;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        
        std::array<VkSubpassDependency, 2> dependencies;
        
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        
        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachmentDescription;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

        VkRenderPass renderPass;

        ASSERT(vkCreateRenderPass(VulkanApplication::GetVKDevice(), &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS, "Failed to create a render pass.");

        shadowMapPass = std::make_shared<RenderPass>(renderPass);
        shadowMapFramebuffer = std::make_shared<Framebuffer>(shadowMapPass, std::vector<VkImageView> {shadowMapImage->GetImageView()}, SHADOW_DIM, SHADOW_DIM);


        model = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\Sponza\\scene.gltf");
        model->SetShadowMap(shadowMapTexture);
        model->Scale(0.005f, 0.005f, 0.005f);
        SetupScenePassConfiguration(model);
        SetupShadowPassConfiguration(model);
        model->SetActiveConfiguration("NormalRenderPass");

        model2 = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\MaleniaHelmet\\scene.gltf");
        model2->SetShadowMap(shadowMapTexture);
        model2->Scale(0.7f, 0.7f, 0.7f);
        SetupScenePassConfiguration(model2);
        SetupShadowPassConfiguration(model2);
        model2->SetActiveConfiguration("NormalRenderPass");
        
	    lightSphere = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\Sphere\\scene.gltf");
        SetupLightSphere(lightSphere);
        lightSphere->SetActiveConfiguration("NormalRenderPass");

        lightSphere->Scale(0.005f, 0.005f, 0.005f);
        auto mat = lightSphere->GetModelMatrix();
        lightSphere->UpdateUniformBuffer(0, &mat, sizeof(mat));

        const float cubeVertices[14 * 6 * 6] = {
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            
             -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
             -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
             -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,

            -1.0f, -1.0f,  1.0f, 0.0f,  0.0f,  1.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            
            -1.0f,  1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
             1.0f,  1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             1.0f,  1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,  0.0f, -1.0f,  0.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,  0.0f, -1.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            
            -1.0f, -1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,  0.0f,  1.0f,  0.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  1.0f,
             1.0f, -1.0f,  1.0f,  0.0f,  1.0f,  0.0f,  0.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f
        };

        const float quadVertices[4 * 14] =
        {
             -1.0f, -1.0f, 0.0f,  0.0f, 1.0f, -1.0f,  0.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
              1.0f, -1.0f, 0.0f,  1.0f, 1.0f, -1.0f,  1.0f,  0.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  0.0f,
              1.0f,  1.0f, 0.0f,  1.0f, 0.0f, -1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f,
             -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, -1.0f,  1.0f,  1.0f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f,  1.0f
        };

        std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

        debugQuad = new Mesh(quadVertices, (size_t)(sizeof(float) * 4 * 14), 4, indices, shadowMapTexture);

        std::vector<DescriptorLayout> dscLayout =
        {
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::UNIFORM_BUFFER,Size::MAT4,ShaderStage::VERTEX},
            DescriptorLayout {Type::TEXTURE_SAMPLER,Size::SHADOWMAP_SAMPLER,ShaderStage::FRAGMENT}
        };


        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE;
        specs.RenderPass = s_ModelRenderPass;
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS;
        specs.EnableBlending = VK_FALSE;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/debugQuadVERT.spv";
        specs.FragmentShaderPath = "shaders/debugQuadFRAG.spv";


        debugQuad->AddConfiguration("NormalRenderPass", specs, dscLayout);
        debugQuad->SetActiveConfiguration("NormalRenderPass");

        debugQuad->Translate(5.0f, 7.4f, 0.0f);
        mat = debugQuad->GetModelMatrix();
        debugQuad->UpdateUniformBuffer(0, &mat, sizeof(mat));


        std::string front   = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\front.jpg");
        std::string back    = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\back.jpg");
        std::string top     = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\top.jpg");
        std::string bottom  = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\bottom.jpg");
        std::string right   = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\right.jpg");
        std::string left    = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\left.jpg");


        std::array<std::string, 6> skyboxTex { right, left, top, bottom, front, back};
        Ref<Anor::CubemapTexture> cubemap = std::make_shared<CubemapTexture>(skyboxTex, VK_FORMAT_R8G8B8A8_SRGB);

        skybox = new Mesh(cubeVertices, (size_t)(sizeof(float) * 14 * 6 * 6), 6 * 6, cubemap);

        SetupSkybox(skybox);
        skybox->SetActiveConfiguration("NormalRenderPass");

        depthMVP = lightProjectionMatrix * lightView * lightModel;
	}
    void OnUpdate()
    {
        rendertime = GetRenderTime();
        float deltaTime = DeltaTime();
        timer += 7.0f * deltaTime;

        // Animating the light
        //directionalLightPosition.x = std::cos(glm::radians(timer )) * 35.0f;
        directionalLightPosition.z = std::sin(glm::radians(timer )) * 9.0f;
        lightView = glm::lookAt(directionalLightPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        depthMVP = lightProjectionMatrix * lightView * lightModel;
        pointLightPosition = glm::vec3(lightSphere->GetModelMatrix()[3][0], lightSphere->GetModelMatrix()[3][1], lightSphere->GetModelMatrix()[3][2]);


        glm::mat4 view = GetCameraViewMatrix();         // Returns the view matrix of the active camera.
        glm::mat4 proj = GetCameraProjectionMatrix();   // Returns the projection matrix of the active camera.
        glm::vec3 cameraPos = GetCameraPosition();      // Returns the position of the active camera.

        glm::mat mat = model->GetModelMatrix();
        model2->Translate(0.0, 0.2f * deltaTime, 0.0f);
        glm::mat mat2 = model2->GetModelMatrix();
        
        BeginDepthPass(shadowMapPass, shadowMapFramebuffer);
        
        model2->SetActiveConfiguration("ShadowMapPass");
        model2->UpdateUniformBuffer(0, &mat2, sizeof(mat2));
        model2->UpdateUniformBuffer(1, &depthMVP, sizeof(depthMVP));
        model2->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.

        model->SetActiveConfiguration("ShadowMapPass");
        model->UpdateUniformBuffer(0, &mat, sizeof(mat));
        model->UpdateUniformBuffer(1, &depthMVP, sizeof(depthMVP));
        model->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.
        
        EndDepthPass();
        

        BeginRenderPass(s_ModelRenderPass);
        
        model->SetActiveConfiguration("NormalRenderPass");
        model2->SetActiveConfiguration("NormalRenderPass");
        
        glm::mat4 skyBoxView = glm::mat4(glm::mat3(view));
        skybox->UpdateUniformBuffer(0, &skyBoxView, sizeof(skyBoxView));
        skybox->UpdateUniformBuffer(1, &proj, sizeof(proj));
        skybox->Draw(GetActiveCommandBuffer());
        

        model2->UpdateUniformBuffer(0, &mat2, sizeof(mat2)); // TO DO: Make this part automated.
        model2->UpdateUniformBuffer(1, &view, sizeof(view));
        model2->UpdateUniformBuffer(2, &proj, sizeof(proj));
        model2->UpdateUniformBuffer(3, &depthMVP, sizeof(depthMVP));
        model2->UpdateUniformBuffer(4, &directionalLightPosition, sizeof(directionalLightPosition));
        model2->UpdateUniformBuffer(5, &cameraPos, sizeof(cameraPos));
        model2->UpdateUniformBuffer(6, &pointLightPosition, sizeof(pointLightPosition));

        model2->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.


        model->UpdateUniformBuffer(0, &mat, sizeof(mat)); // TO DO: Make this part automated.
        model->UpdateUniformBuffer(1, &view, sizeof(view));
        model->UpdateUniformBuffer(2, &proj, sizeof(proj));
        model->UpdateUniformBuffer(3, &depthMVP, sizeof(depthMVP));
        model->UpdateUniformBuffer(4, &directionalLightPosition, sizeof(directionalLightPosition));
        model->UpdateUniformBuffer(5, &cameraPos, sizeof(cameraPos));
        model->UpdateUniformBuffer(6, &pointLightPosition, sizeof(pointLightPosition));

        model->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.
        
        debugQuad->UpdateUniformBuffer(1, &proj, sizeof(proj));
        debugQuad->UpdateUniformBuffer(2, &view, sizeof(view));
        glm::mat4 temp = lightProjectionMatrix * lightView * glm::mat4(1.0);
        debugQuad->UpdateUniformBuffer(3, &temp, sizeof(temp));
        debugQuad->DrawIndexed(GetActiveCommandBuffer());


        lightSphere->UpdateUniformBuffer(1, &view, sizeof(view));
        lightSphere->UpdateUniformBuffer(2, &proj, sizeof(proj));
        lightSphere->Translate(0.0f, 0.0f, 10.0f * deltaTime);
        mat = lightSphere->GetModelMatrix();
        lightSphere->UpdateUniformBuffer(0, &mat, sizeof(mat));
        lightSphere->DrawIndexed(GetActiveCommandBuffer());
        
        EndRenderPass();

    }
    void OnCleanup()
    {
        delete model;
        delete model2;
        delete skybox;
        delete lightSphere;
        delete debugQuad;
    }
    void OnWindowResize()
    {
        model->OnResize();
        skybox->OnResize();
        lightSphere->OnResize();
        model2->OnResize();
        debugQuad->OnResize();
    }
    float DeltaTime()
    {
        float currentFrameRenderTime, deltaTime;
        currentFrameRenderTime = rendertime;
        deltaTime = currentFrameRenderTime - m_LastFrameRenderTime;
        m_LastFrameRenderTime = currentFrameRenderTime;
        return deltaTime;
    }
};
int main()
{
	MyApplication app;
	app.Run();
	return 0;
}