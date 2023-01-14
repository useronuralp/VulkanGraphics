#include "OVKLib.h"

#define MAX_POINT_LIGHT_COUNT 10
#define SHADOW_DIM 10000 
#define POUNT_SHADOW_DIM 1000
#define MAX_FRAMES_IN_FLIGHT 1
int CURRENT_FRAME = 0;

using namespace OVK;
class MyApplication : public OVK::VulkanApplication
{
public:
    MyApplication(uint32_t framesInFlight) : VulkanApplication(framesInFlight){}
private:
    bool pointLightShadows = true;
    struct GlobalParametersUBO
    {
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
        glm::vec4 far_plane;
        glm::vec4 pointLightCount;
    };
public:
#pragma region ClearValues
    VkClearValue                depthPassClearValue;
    std::array<VkClearValue, 2> clearValues;
#pragma endregion

#pragma region Images&Framebuffers
    Ref<Framebuffer> directionalShadowMapFramebuffer;
    Ref<Framebuffer> HDRFramebuffer;
    std::vector<Ref<Framebuffer>> pointShadowMapFramebuffers;
    // Attachments. Each framebuffer can have multiple attachments.
    Ref<Image>       directionalShadowMapImage;
    Ref<Image>       HDRColorImage;
    Ref<Image>       HDRDepthImage;
    Ref<Image>       particleTexture;
    Ref<Image>       dustTexture;
    Ref<Image>       fireTexture;
    std::vector<Ref<Image>> pointShadowMaps;
#pragma endregion

#pragma region RenderPassBeginInfos
    // Render pass begin infos.
    VkRenderPassBeginInfo depthPassBeginInfo;
    VkRenderPassBeginInfo pointShadowPassBeginInfo;
    VkRenderPassBeginInfo finalScenePassBeginInfo;
    VkRenderPassBeginInfo HDRRenderPassBeginInfo;
#pragma endregion

#pragma region  RenderPasses
    // Render passes
    VkRenderPass shadowMapRenderPass;
    VkRenderPass pointShadowRenderPass;
    VkRenderPass HDRRenderPass;
#pragma endregion 

#pragma region Layouts
    // Descriptor Layouts
    Ref<DescriptorLayout> oneSamplerLayout;
    Ref<DescriptorLayout> emissiveLayout;
    Ref<DescriptorLayout> PBRLayout;
    Ref<DescriptorLayout> skyboxLayout;
    Ref<DescriptorLayout> cubeLayout;
    Ref<DescriptorLayout> particleSystemLayout;
#pragma endregion

#pragma region Pools
    // Descriptor Pools
    Ref<DescriptorPool> pool;
    Ref<DescriptorPool> pool2;
#pragma endregion

#pragma region Pipelines
    // Pipelines
    Ref<Pipeline> EmissiveObjectPipeline;
    Ref<Pipeline> finalPassPipeline;
    Ref<Pipeline> pipeline;
    Ref<Pipeline> pointShadowPassPipeline;
    Ref<Pipeline> shadowPassPipeline;
    Ref<Pipeline> skyboxPipeline;
    Ref<Pipeline> cubePipeline;
    Ref<Pipeline> particleSystemPipeline;
#pragma endregion

#pragma region ModelsANDParticleSystems
    // Models
    Model* model;
    Model* model2;
    Model* model3;
    Model* torch;
    Model* skybox;
    Model* cube;

    ParticleSystem* fireSparks;
    ParticleSystem* fireBase;

    ParticleSystem* fireSparks2;
    ParticleSystem* fireBase2;

    ParticleSystem* fireSparks3;
    ParticleSystem* fireBase3;

    ParticleSystem* fireSparks4;
    ParticleSystem* fireBase4;

    ParticleSystem* ambientParticles;
#pragma endregion

#pragma region UBOs
    GlobalParametersUBO         globalParametersUBO;
    VkBuffer                    globalParametersUBOBuffer;
    VkDeviceMemory              globalParametersUBOBufferMemory;
    void*                       mappedGlobalParametersModelUBOBuffer;
#pragma endregion

    // Others
    VkCommandBuffer                     cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool                       cmdPool;
    Ref<Bloom>                          bloomAgent;
    VkSampler                           finalPassSampler;
    VkDescriptorSet                     finalPassDescriptorSet;


    std::random_device                  rd; // obtain a random number from hardware
    std::mt19937                        gen; // seed the generator
    std::uniform_real_distribution<>    distr;

    VkVertexInputBindingDescription bindingDescription{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

    VkVertexInputBindingDescription bindingDescription2{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions2{};

    VkVertexInputBindingDescription bindingDescription3{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions3{};

    VkVertexInputBindingDescription bindingDescription4{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions4{};

    VkVertexInputBindingDescription bindingDescription5{};
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions5{};

    glm::mat4 torch1modelMatrix {1.0};
    glm::mat4 torch2modelMatrix {1.0};
    glm::mat4 torch3modelMatrix {1.0};
    glm::mat4 torch4modelMatrix {1.0};

    float   lightFlickerRate = 0.07f;
    float   aniamtionRate = 0.013888888f;
    int     currentAnimationFrame = 0;
    float   timer = 0.0f;

    glm::vec4 directionalLightPosition = glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f);

    float near_plane = 1.0f;
    float far_plane = 100.0f;

    float point_near_plane = 0.1f;
    float point_far_plane = 100.0f;

    int   frameCount = 0;

    glm::mat4 directionalLightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, near_plane, far_plane);
    glm::mat4 pointLightProjectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, point_near_plane, point_far_plane);

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame = GetRenderTime();

    // Helpers
    void WriteDescriptorSetWithGlobalUBO(Model* model)
    {
        for (int i = 0; i < model->GetMeshCount(); i++)
        {
            // Write the descriptor set.
            VkWriteDescriptorSet descriptorWrite{};
            VkDescriptorBufferInfo bufferInfo{};

            bufferInfo.buffer = globalParametersUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GlobalParametersUBO);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = model->GetMeshes()[i]->GetDescriptorSet();
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }
    void WriteDescriptorSet_Skybox(Model* model)
    {
        // Write the descriptor set.
        VkWriteDescriptorSet descriptorWrite{};
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer = globalParametersUBOBuffer;
        bufferInfo.offset = sizeof(glm::mat4);
        bufferInfo.range = sizeof(glm::mat4);

        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = model->GetMeshes()[0]->GetDescriptorSet();
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    void WriteDescriptorSet_Cube(Model* model)
    {
        // Write the descriptor set.
        VkWriteDescriptorSet descriptorWrite{};
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer = globalParametersUBOBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(glm::mat4) + sizeof(glm::mat4);

        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = model->GetMeshes()[0]->GetDescriptorSet();
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        descriptorWrite.pImageInfo = nullptr; // Optional
        descriptorWrite.pTexelBufferView = nullptr; // Optional

        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
    }
    void WriteDescriptorSet_EmissiveObject(Model* model)
    {
        for (int i = 0; i < model->GetMeshCount(); i++)
        {
            // Write the descriptor set.
            VkWriteDescriptorSet descriptorWrite{};
            VkDescriptorBufferInfo bufferInfo{};

            bufferInfo.buffer = globalParametersUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::mat4) * 2;

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = model->GetMeshes()[i]->GetDescriptorSet();
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void SetupPBRPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = PBRLayout;
        specs.pRenderPass = &HDRRenderPass;
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/PBRShaderVERT.spv";
        specs.FragmentShaderPath = "shaders/PBRShaderFRAG.spv";


        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        specs.PushConstantRanges = { pcRange };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions.resize(5);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(glm::vec3);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = sizeof(glm::vec3) + sizeof(glm::vec2);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[4].offset = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3);


        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription;
        specs.VertexInputAttributeCount = attributeDescriptions.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions.data();

        pipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupFinalPassPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = oneSamplerLayout;
        specs.pRenderPass = &s_Swapchain->GetSwapchainRenderPass();
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/quadRenderVERT.spv";
        specs.FragmentShaderPath = "shaders/swapchainFRAG.spv";
        
        
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        
        specs.ColorBlendAttachmentState = colorBlendAttachment;
        
        finalPassPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupShadowPassPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = PBRLayout;
        specs.pRenderPass = &shadowMapRenderPass;
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 1.25f;
        specs.DepthBiasSlopeFactor = 1.75f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = VK_TRUE;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/shadowPassVERT.spv";
        specs.ViewportHeight = SHADOW_DIM;
        specs.ViewportWidth = SHADOW_DIM;

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        specs.PushConstantRanges = { pcRange };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;


        bindingDescription2.binding = 0;
        bindingDescription2.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
        bindingDescription2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions2.resize(1);

        attributeDescriptions2[0].binding = 0;
        attributeDescriptions2[0].location = 0;
        attributeDescriptions2[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions2[0].offset = 0;

        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription2;
        specs.VertexInputAttributeCount = attributeDescriptions2.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions2.data();

        shadowPassPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupPointShadowPassPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = PBRLayout;
        specs.pRenderPass = &pointShadowRenderPass;
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 1.25f;
        specs.DepthBiasSlopeFactor = 1.75f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = VK_FALSE;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/pointShadowPassVERT.spv";
        specs.FragmentShaderPath = "shaders/pointShadowPassFRAG.spv";
        specs.GeometryShaderPath = "shaders/pointShadowPassGEOM.spv";
        specs.ViewportHeight = POUNT_SHADOW_DIM;
        specs.ViewportWidth = POUNT_SHADOW_DIM;

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPushConstantRange pcRange2;
        pcRange2.offset = sizeof(glm::mat4);
        pcRange2.size = sizeof(glm::vec4);
        pcRange2.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT;

        VkPushConstantRange pcRange3;
        pcRange3.offset = sizeof(glm::mat4) + sizeof(glm::vec4);
        pcRange3.size = sizeof(glm::vec4) + sizeof(glm::vec4);
        pcRange3.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        specs.PushConstantRanges = { pcRange , pcRange2 , pcRange3 };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription2.binding = 0;
        bindingDescription2.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
        bindingDescription2.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions2.resize(1);

        attributeDescriptions2[0].binding = 0;
        attributeDescriptions2[0].location = 0;
        attributeDescriptions2[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions2[0].offset = 0;

        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription2;
        specs.VertexInputAttributeCount = attributeDescriptions2.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions2.data();

        pointShadowPassPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupSkyboxPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = skyboxLayout;
        specs.pRenderPass = &HDRRenderPass;
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_FALSE;
        specs.EnableDepthWriting = VK_FALSE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/cubemapVERT.spv";
        specs.FragmentShaderPath = "shaders/cubemapFRAG.spv";

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        specs.PushConstantRanges = { pcRange };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription3.binding = 0;
        bindingDescription3.stride = sizeof(glm::vec3);
        bindingDescription3.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions3.resize(1);
        // For position
        attributeDescriptions3[0].binding = 0;
        attributeDescriptions3[0].location = 0;
        attributeDescriptions3[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions3[0].offset = 0;

        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription3;
        specs.VertexInputAttributeCount = attributeDescriptions3.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions3.data();

        skyboxPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupCubePipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = cubeLayout;
        specs.pRenderPass = &HDRRenderPass;
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/emissiveShaderVERT.spv";
        specs.FragmentShaderPath = "shaders/emissiveShaderFRAG.spv";

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4) + sizeof(glm::vec4) + sizeof(glm::vec4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        specs.PushConstantRanges = { pcRange };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription3.binding = 0;
        bindingDescription3.stride = sizeof(glm::vec3);
        bindingDescription3.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions3.resize(1);
        // For position
        attributeDescriptions3[0].binding = 0;
        attributeDescriptions3[0].location = 0;
        attributeDescriptions3[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions3[0].offset = 0;

        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription3;
        specs.VertexInputAttributeCount = attributeDescriptions3.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions3.data();

        cubePipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupParticleSystemPipeline()
    {
        Pipeline::Specs particleSpecs{};
        Ref<DescriptorLayout> layout = particleSystemLayout;
        particleSpecs.DescriptorLayout = layout;
        particleSpecs.pRenderPass = &HDRRenderPass;
        particleSpecs.CullMode = VK_CULL_MODE_BACK_BIT;
        particleSpecs.DepthBiasClamp = 0.0f;
        particleSpecs.DepthBiasConstantFactor = 0.0f;
        particleSpecs.DepthBiasSlopeFactor = 0.0f;
        particleSpecs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        particleSpecs.EnableDepthBias = false;
        particleSpecs.EnableDepthTesting = VK_TRUE;
        particleSpecs.EnableDepthWriting = VK_FALSE;
        particleSpecs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        particleSpecs.PolygonMode = VK_POLYGON_MODE_FILL;
        particleSpecs.VertexShaderPath = "shaders/particleVERT.spv";
        particleSpecs.FragmentShaderPath = "shaders/particleFRAG.spv";
        particleSpecs.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;



        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::vec4);
        pcRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        particleSpecs.PushConstantRanges = { pcRange };

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        particleSpecs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription4.binding = 0;
        bindingDescription4.stride = sizeof(Particle);
        bindingDescription4.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions4.resize(9);
        attributeDescriptions4[0].binding = 0;
        attributeDescriptions4[0].location = 0;
        attributeDescriptions4[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions4[0].offset = offsetof(Particle, Position);

        attributeDescriptions4[1].binding = 0;
        attributeDescriptions4[1].location = 1;
        attributeDescriptions4[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions4[1].offset = offsetof(Particle, Color);

        attributeDescriptions4[2].binding = 0;
        attributeDescriptions4[2].location = 2;
        attributeDescriptions4[2].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[2].offset = offsetof(Particle, Alpha);

        attributeDescriptions4[3].binding = 0;
        attributeDescriptions4[3].location = 3;
        attributeDescriptions4[3].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[3].offset = offsetof(Particle, SizeRadius);

        attributeDescriptions4[4].binding = 0;
        attributeDescriptions4[4].location = 4;
        attributeDescriptions4[4].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[4].offset = offsetof(Particle, Rotation);

        attributeDescriptions4[5].binding = 0;
        attributeDescriptions4[5].location = 5;
        attributeDescriptions4[5].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[5].offset = offsetof(Particle, RowOffset);

        attributeDescriptions4[6].binding = 0;
        attributeDescriptions4[6].location = 6;
        attributeDescriptions4[6].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[6].offset = offsetof(Particle, ColumnOffset);

        attributeDescriptions4[7].binding = 0;
        attributeDescriptions4[7].location = 7;
        attributeDescriptions4[7].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[7].offset = offsetof(Particle, RowCellSize);

        attributeDescriptions4[8].binding = 0;
        attributeDescriptions4[8].location = 8;
        attributeDescriptions4[8].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions4[8].offset = offsetof(Particle, ColumnCellSize);


        particleSpecs.VertexInputBindingCount = 1;
        particleSpecs.pVertexInputBindingDescriptions = &bindingDescription4;
        particleSpecs.VertexInputAttributeCount = attributeDescriptions4.size();
        particleSpecs.pVertexInputAttributeDescriptons = attributeDescriptions4.data();

        particleSystemPipeline = std::make_shared<Pipeline>(particleSpecs);
    }
    void SetupEmissiveObjectPipeline()
    {
        // Emissive object pipeline.
        Pipeline::Specs specs{};
        specs.DescriptorLayout = emissiveLayout;
        specs.pRenderPass = &HDRRenderPass;
        specs.VertexShaderPath = "shaders/emissiveShaderVERT.spv";
        specs.FragmentShaderPath = "shaders/emissiveShaderFRAG.spv";

        VkPushConstantRange pcRange;
        pcRange.offset = 0;
        pcRange.size = sizeof(glm::mat4) + sizeof(glm::vec4);
        pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        specs.PushConstantRanges = { pcRange };

        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        specs.ColorBlendAttachmentState = colorBlendAttachment;

        bindingDescription5.binding = 0;
        bindingDescription5.stride = sizeof(glm::vec3);
        bindingDescription5.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attributeDescriptions5.resize(1);
        attributeDescriptions5[0].binding = 0;
        attributeDescriptions5[0].location = 0;
        attributeDescriptions5[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions5[0].offset = 0;

        specs.VertexInputBindingCount = 1;
        specs.pVertexInputBindingDescriptions = &bindingDescription5;
        specs.VertexInputAttributeCount = attributeDescriptions5.size();
        specs.pVertexInputAttributeDescriptons = attributeDescriptions5.data();

        EmissiveObjectPipeline = std::make_shared<Pipeline>(specs);
    }

    void SetupParticleSystems()
    {
        particleTexture = std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib/textures/spark.png") }, VK_FORMAT_R8G8B8A8_SRGB);
        fireTexture = std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib/textures/fire_sprite_sheet.png")}, VK_FORMAT_R8G8B8A8_SRGB);
        dustTexture = std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib/textures/dust.png") }, VK_FORMAT_R8G8B8A8_SRGB);

        ParticleSpecs specs{};
        specs.ParticleCount = 10;
        specs.EnableNoise = true;
        specs.TrailLength = 2;
        specs.SphereRadius = 0.05f;
        specs.ImmortalParticle = false;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks = new ParticleSystem(specs, particleTexture, particleSystemLayout, pool);
        fireSparks->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.28f, torch1modelMatrix[3].z - 0.03f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase = new ParticleSystem(specs , fireTexture, particleSystemLayout, pool);
        fireBase->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
        fireBase->RowOffset = 0.0f;
        fireBase->RowCellSize = 0.0833333333333333333333f;
        fireBase->ColumnCellSize = 0.166666666666666f;
        fireBase->ColumnOffset = 0.0f;


        specs.ParticleCount = 10;
        specs.EnableNoise = true;
        specs.TrailLength = 2;
        specs.SphereRadius = 0.05f;
        specs.ImmortalParticle = false;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks2 = new ParticleSystem(specs, particleTexture, particleSystemLayout, pool);
        fireSparks2->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.28f, torch2modelMatrix[3].z + 0.03f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase2 = new ParticleSystem(specs, fireTexture, particleSystemLayout, pool);
        fireBase2->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
        fireBase2->RowOffset = 0.0f;
        fireBase2->RowCellSize = 0.0833333333333333333333f;
        fireBase2->ColumnCellSize = 0.166666666666666f;
        fireBase2->ColumnOffset = 0.0f;

        specs.ParticleCount = 10;
        specs.EnableNoise = true;
        specs.TrailLength = 2;
        specs.SphereRadius = 0.05f;
        specs.ImmortalParticle = false;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks3 = new ParticleSystem(specs, particleTexture, particleSystemLayout, pool);
        fireSparks3->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.28f, torch3modelMatrix[3].z - 0.03f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase3 = new ParticleSystem(specs, fireTexture, particleSystemLayout, pool);
        fireBase3->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
        fireBase3->RowOffset = 0.0f;
        fireBase3->RowCellSize = 0.0833333333333333333333f;
        fireBase3->ColumnCellSize = 0.166666666666666f;
        fireBase3->ColumnOffset = 0.0f;

        specs.ParticleCount = 10;
        specs.EnableNoise = true;
        specs.TrailLength = 2;
        specs.SphereRadius = 0.05f;
        specs.ImmortalParticle = false;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks4 = new ParticleSystem(specs, particleTexture, particleSystemLayout, pool);
        fireSparks4->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.28f, torch4modelMatrix[3].z + 0.03f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase4 = new ParticleSystem(specs, fireTexture, particleSystemLayout, pool);
        fireBase4->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
        fireBase4->RowOffset = 0.0f;
        fireBase4->RowCellSize = 0.0833333333333333333333f;
        fireBase4->ColumnCellSize = 0.166666666666666f;
        fireBase4->ColumnOffset = 0.0f;

        specs.ParticleCount = 500;
        specs.EnableNoise = true;
        specs.TrailLength = 0;
        specs.SphereRadius = 5.0f;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(0, 2.0f, 0);
        specs.ParticleMinLifetime = 5.0f;
        specs.ParticleMaxLifetime = 10.0f;
        specs.MinVel = glm::vec3(-0.3f, -0.3f, -0.3f);
        specs.MaxVel = glm::vec3(0.3f, 0.3f, 0.3f);

        ambientParticles = new ParticleSystem(specs, dustTexture, particleSystemLayout, pool);
        ambientParticles->SetUBO(globalParametersUBOBuffer, (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3), 0);
    }

    void CreateHDRFramebuffer()
    {
        HDRColorImage = std::make_shared<Image>(VulkanApplication::s_Surface->GetVKExtent().width, VulkanApplication::s_Surface->GetVKExtent().height,
            VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, ImageType::COLOR);


        HDRDepthImage = std::make_shared<Image>(VulkanApplication::s_Surface->GetVKExtent().width, VulkanApplication::s_Surface->GetVKExtent().height,
            Utils::FindDepthFormat(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, ImageType::DEPTH);

        std::vector<VkImageView> attachments =
        {
            HDRColorImage->GetImageView(),
            HDRDepthImage->GetImageView()
        };

        HDRFramebuffer = std::make_shared<Framebuffer>(HDRRenderPass, attachments, VulkanApplication::s_Surface->GetVKExtent().width, VulkanApplication::s_Surface->GetVKExtent().height);
    }

    void CreateHDRRenderPass()
    {
        // HDR render pass.
        VkAttachmentDescription colorAttachmentDescription;
        VkAttachmentReference colorAttachmentRef;

        VkAttachmentDescription depthAttachmentDescription;
        VkAttachmentReference depthAttachmentRef;

        colorAttachmentDescription.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        colorAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDescription.flags = 0;
        colorAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // Color att. reference.
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // A single depth attachment for depth testing / occlusion.
        depthAttachmentDescription.format = Utils::FindDepthFormat();
        depthAttachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachmentDescription.flags = 0;
        depthAttachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Depth att. reference.
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        // TO DO: I don't know if these synchronization values are correct. UNDERSTAND THEM BETTER.
        dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachmentDescriptions;
        attachmentDescriptions[0] = colorAttachmentDescription;
        attachmentDescriptions[1] = depthAttachmentDescription;


        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size()); // Number of attachments.
        renderPassInfo.pAttachments = attachmentDescriptions.data(); // An array with the size of "attachmentCount".
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        ASSERT(vkCreateRenderPass(VulkanApplication::s_Device->GetVKDevice(), &renderPassInfo, nullptr, &HDRRenderPass) == VK_SUCCESS, "Failed to create a render pass.");
    }
    void CreateShadowRenderPass()
    {
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

        VkSubpassDependency dependency;

        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachmentDescription;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        ASSERT(vkCreateRenderPass(s_Device->GetVKDevice(), &renderPassInfo, nullptr, &shadowMapRenderPass) == VK_SUCCESS, "Failed to create a render pass.");
    }
    void CreatePointShadowRenderPass()
    {
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

        VkSubpassDependency dependency;

        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &depthAttachmentDescription;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        ASSERT(vkCreateRenderPass(s_Device->GetVKDevice(), &renderPassInfo, nullptr, &pointShadowRenderPass) == VK_SUCCESS, "Failed to create a render pass.");
    }

    // Shadow map generators
    void DirectionalShadowPass()
    {
        glm::mat4 mat = model->GetTransform();
        glm::mat4 mat2 = model2->GetTransform();

        // Start shadow pass.---------------------------------------------
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline);

        // Render the objects you want to cast shadows.
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
        model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
        model2->DrawIndexed(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout());

        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
        // End shadow pass.---------------------------------------------
    }
    void PointLightShadowPass()
    {
        glm::mat4 mat = model->GetTransform();
        glm::mat4 mat2 = model2->GetTransform();

        // Start point shadow pass.--------------------
        for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
        {
            pointShadowPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            pointShadowPassBeginInfo.renderPass = pointShadowRenderPass;
            pointShadowPassBeginInfo.framebuffer = pointShadowMapFramebuffers[i]->GetVKFramebuffer();
            pointShadowPassBeginInfo.renderArea.offset = { 0, 0 };
            pointShadowPassBeginInfo.renderArea.extent.width = pointShadowMapFramebuffers[i]->GetWidth();
            pointShadowPassBeginInfo.renderArea.extent.height = pointShadowMapFramebuffers[i]->GetHeight();
            pointShadowPassBeginInfo.clearValueCount = 1;
            pointShadowPassBeginInfo.pClearValues = &depthPassClearValue;
            
            CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], pointShadowPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, pointShadowPassPipeline);

            glm::vec3 position = glm::vec3(globalParametersUBO.pointLightPositions[i].x, globalParametersUBO.pointLightPositions[i].y, globalParametersUBO.pointLightPositions[i].z);

            globalParametersUBO.shadowMatrices[i][0] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][1] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][2] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
            globalParametersUBO.shadowMatrices[i][3] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
            globalParametersUBO.shadowMatrices[i][4] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
            globalParametersUBO.shadowMatrices[i][5] = pointLightProjectionMatrix * glm::lookAt(position, position + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));


            struct PC
            {
                glm::vec4 lightPos;
                glm::vec4 farPlane;
            };

            glm::vec4 pointLightIndex = glm::vec4(i);

            PC pc;
            pc.lightPos = glm::vec4(position, 1.0f);
            pc.farPlane = glm::vec4(point_far_plane);

            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &pointLightIndex);
            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec4) + sizeof(glm::vec4), &pc);
            model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout());

            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_GEOMETRY_BIT, sizeof(glm::mat4), sizeof(glm::vec4), &pointLightIndex);
            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(glm::mat4) + sizeof(glm::vec4), sizeof(glm::vec4) + sizeof(glm::vec4), &pc);
            model2->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pointShadowPassPipeline->GetPipelineLayout());

            CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
            // End point shadow pass.----------------------
        }
    }
private:
    void OnVulkanInit() override
    {
        // Set the device extensions we want Vulkan to enable.
        SetDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME });

        // Set the parameters of the main camera.
        SetCameraConfiguration(45.0f, 0.1f, 1500.0f);
    }

    void OnStart() override
    {
        globalParametersUBO.pointLightCount = glm::vec4(5);

        // Set the point light colors here.
        globalParametersUBO.pointLightColors[0] = glm::vec4(0.97, 0.76, 0.46, 1.0);
        globalParametersUBO.pointLightColors[1] = glm::vec4(0.97, 0.76, 0.46, 1.0);
        globalParametersUBO.pointLightColors[2] = glm::vec4(0.97, 0.76, 0.46, 1.0);
        globalParametersUBO.pointLightColors[3] = glm::vec4(0.97, 0.76, 0.46, 1.0);
        globalParametersUBO.pointLightColors[4] = glm::vec4(1.0, 0.0, 0.0, 1.0);
        
        CurlNoise::SetCurlSettings(false, 4.0f, 6, 1.0, 0.0);
        pointShadowMaps.resize(globalParametersUBO.pointLightCount.x);
        pointShadowMapFramebuffers.resize(globalParametersUBO.pointLightCount.x);

        std::vector<DescriptorBindingSpecs> hdrLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(GlobalParametersUBO),         1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,    0},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 1},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_NORMAL,             UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 2},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC,  UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 3},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_SHADOWMAP,          UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 4},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_POINTSHADOWMAP,     UINT64_MAX,                          5, VK_SHADER_STAGE_FRAGMENT_BIT , 5},
        };

        std::vector<DescriptorBindingSpecs> SkyboxLayout
        {
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,                      sizeof(glm::mat4),                   1, VK_SHADER_STAGE_VERTEX_BIT  ,  0},
            DescriptorBindingSpecs {Type::TEXTURE_SAMPLER_CUBEMAP,             UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT,  1}
        };

        std::vector<DescriptorBindingSpecs> ParticleSystemLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     (sizeof(glm::mat4) * 3) + (sizeof(glm::vec4) * 3),         1, VK_SHADER_STAGE_VERTEX_BIT   , 0}, // Index 0
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 1}, // Index 3
        };

        std::vector<DescriptorBindingSpecs> OneSamplerLayout
        {
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, VK_SHADER_STAGE_FRAGMENT_BIT , 0},
        };

        std::vector<DescriptorBindingSpecs> EmissiveLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(glm::mat4) * 2,         1, VK_SHADER_STAGE_VERTEX_BIT,    0},
        };
        
        std::vector<DescriptorBindingSpecs> CubeLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(glm::mat4) * 2,   1, VK_SHADER_STAGE_VERTEX_BIT,    0},
        };


        // We are using only a single pool for all our descriptor sets. Set it up here.
        std::vector<VkDescriptorType> types;
        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        pool = std::make_shared<DescriptorPool>(500, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        pool2 = std::make_shared<DescriptorPool>(500, types);
        

        // Descriptor Set Layouts
        particleSystemLayout = std::make_shared<DescriptorLayout>(ParticleSystemLayout);
        skyboxLayout = std::make_shared<DescriptorLayout>(SkyboxLayout);
        cubeLayout = std::make_shared<DescriptorLayout>(CubeLayout);
        PBRLayout = std::make_shared<DescriptorLayout>(hdrLayout);
        oneSamplerLayout = std::make_shared<DescriptorLayout>(OneSamplerLayout);
        emissiveLayout = std::make_shared<DescriptorLayout>(EmissiveLayout);


        // Following are the global Uniform Buffes shared by all shaders.
        Utils::CreateVKBuffer(sizeof(GlobalParametersUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, globalParametersUBOBuffer, globalParametersUBOBufferMemory);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBufferMemory, 0, sizeof(GlobalParametersUBO), 0, &mappedGlobalParametersModelUBOBuffer);
        
        // Create an image for the shadowmap. We will render to this image when we are doing a shadow pass.
        directionalShadowMapImage = std::make_shared<Image>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), ImageType::DEPTH);

        for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
        {
            pointShadowMaps[i] = std::make_shared<Image>(POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, VK_FORMAT_D32_SFLOAT, (VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT),
                ImageType::DEPTH_CUBEMAP);
        }

        // Allocate final pass descriptor Set.
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &oneSamplerLayout->GetDescriptorLayout();

        VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &finalPassDescriptorSet);
        ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

        // Setup resources.
        CreateHDRRenderPass();
        CreateShadowRenderPass();
        CreatePointShadowRenderPass();

        CreateHDRFramebuffer();

        SetupFinalPassPipeline();
        SetupPBRPipeline();
        SetupShadowPassPipeline();
        SetupSkyboxPipeline();
        SetupCubePipeline();
        SetupPointShadowPassPipeline();
        SetupEmissiveObjectPipeline();
        SetupParticleSystemPipeline();



        // Directional light shadowmap framebuffer.
        std::vector<VkImageView> attachments =
        {
            directionalShadowMapImage->GetImageView()
        };
        
        directionalShadowMapFramebuffer = std::make_shared<Framebuffer>(shadowMapRenderPass, attachments, SHADOW_DIM, SHADOW_DIM);
        

        // Framebuffers need for point light shadows. (Dependent on the number of point lights in the scene)
        for (int i = 0; i < globalParametersUBO.pointLightCount.x; i++)
        {

            attachments =
            {
                pointShadowMaps[i]->GetImageView()
            };


            pointShadowMapFramebuffers[i] = std::make_shared<Framebuffer>(pointShadowRenderPass, attachments, POUNT_SHADOW_DIM, POUNT_SHADOW_DIM, 6);
        }

        // Loading the model Sponza
        model = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\Sponza\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
            pool, PBRLayout, directionalShadowMapImage, pointShadowMaps );
        model->Scale(0.005f, 0.005f, 0.005f);
        WriteDescriptorSetWithGlobalUBO(model);

        // Loading the model Malenia's Helmet.
        model2 = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\MaleniaHelmet\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
            pool, PBRLayout, directionalShadowMapImage, pointShadowMaps);
        model2->Translate(0.0, 2.0f, 0.0);
        model2->Rotate(90, 0, 1, 0);
        model2->Scale(0.7f, 0.7f, 0.7f);
        WriteDescriptorSetWithGlobalUBO(model2);

        torch = new Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\torch\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV,
            pool, PBRLayout, directionalShadowMapImage, pointShadowMaps);


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
        WriteDescriptorSetWithGlobalUBO(torch);

        SetupParticleSystems();

        // Set the positions of the point lights in the scene we have 4 torches.
        globalParametersUBO.pointLightPositions[0] = glm::vec4(glm::vec3(torch1modelMatrix[3].x, torch1modelMatrix[3].y + 0.22f, torch1modelMatrix[3].z - 0.02f), 1.0f);
        globalParametersUBO.pointLightPositions[1] = glm::vec4(glm::vec3(torch2modelMatrix[3].x, torch2modelMatrix[3].y + 0.22f, torch2modelMatrix[3].z + 0.02f), 1.0f);
        globalParametersUBO.pointLightPositions[2] = glm::vec4(glm::vec3(torch3modelMatrix[3].x, torch3modelMatrix[3].y + 0.22f, torch3modelMatrix[3].z - 0.02f), 1.0f);
        globalParametersUBO.pointLightPositions[3] = glm::vec4(glm::vec3(torch4modelMatrix[3].x, torch4modelMatrix[3].y + 0.22f, torch4modelMatrix[3].z + 0.02f), 1.0f);

        globalParametersUBO.pointLightPositions[4] = glm::vec4(-0.3f, 3.190, -0.180, 1.0f);
        globalParametersUBO.pointLightIntensities[4] = glm::vec4(50.0f);
        globalParametersUBO.directionalLightIntensity.x = 10.0;

        globalParametersUBO.far_plane = glm::vec4(point_far_plane);


        model3 = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\sword\\scene.gltf", LOAD_VERTEX_POSITIONS, pool, emissiveLayout);
        model3->Translate(-2, 7, 0);
        model3->Rotate(54, 0, 0, 1);
        model3->Rotate(90, 0, 1, 0);
        model3->Scale(0.7f, 0.7f, 0.7f);
        WriteDescriptorSet_EmissiveObject(model3);


        // Vertex data for the skybox.
        const uint32_t vertexCount = 3 * 6 * 6;
        const float cubeVertices[vertexCount] = {
            -1.0f,  1.0f, -1.0f, 
            -1.0f, -1.0f, -1.0f, 
             1.0f, -1.0f, -1.0f, 
             1.0f, -1.0f, -1.0f, 
             1.0f,  1.0f, -1.0f, 
            -1.0f,  1.0f, -1.0f, 
            
             -1.0f, -1.0f,  1.0f,
             -1.0f, -1.0f, -1.0f,
             -1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f, -1.0f,
             -1.0f,  1.0f,  1.0f,
             -1.0f, -1.0f,  1.0f,
        
             1.0f, -1.0f, -1.0f, 
             1.0f, -1.0f,  1.0f, 
             1.0f,  1.0f,  1.0f, 
             1.0f,  1.0f,  1.0f, 
             1.0f,  1.0f, -1.0f, 
             1.0f, -1.0f, -1.0f, 
        
            -1.0f, -1.0f,  1.0f, 
            -1.0f,  1.0f,  1.0f, 
             1.0f,  1.0f,  1.0f, 
             1.0f,  1.0f,  1.0f, 
             1.0f, -1.0f,  1.0f, 
            -1.0f, -1.0f,  1.0f, 
            
            -1.0f,  1.0f, -1.0f, 
             1.0f,  1.0f, -1.0f, 
             1.0f,  1.0f,  1.0f, 
             1.0f,  1.0f,  1.0f, 
            -1.0f,  1.0f,  1.0f, 
            -1.0f,  1.0f, -1.0f, 
            
            -1.0f, -1.0f, -1.0f, 
            -1.0f, -1.0f,  1.0f, 
             1.0f, -1.0f, -1.0f, 
             1.0f, -1.0f, -1.0f, 
            -1.0f, -1.0f,  1.0f, 
             1.0f, -1.0f,  1.0f, 
        };
        
        // Loading the necessary images for the skybox one by one.
        std::string front   = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\front.png");
        std::string back    = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\back.png");
        std::string top     = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\top.png");
        std::string bottom  = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\bottom.png");
        std::string right   = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\right.png");
        std::string left    = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Night\\left.png");
        
        // Set up the 6 sided texture for the skybox by using the above images.
        std::vector<std::string> skyboxTex { right, left, top, bottom, front, back};
        Ref<OVK::Image> cubemap = std::make_shared<Image>(skyboxTex, VK_FORMAT_R8G8B8A8_SRGB);
        

        // Create the mesh for the skybox.
        skybox = new Model(cubeVertices, vertexCount, cubemap, pool, skyboxLayout);
        WriteDescriptorSet_Skybox(skybox);

        // A cube model to depict/debug point lights.
        cube = new Model(cubeVertices, vertexCount, nullptr, pool2, cubeLayout);
        WriteDescriptorSet_Cube(cube);


        // Shadow pass begin Info.
        depthPassClearValue = { 1.0f, 0.0 };
        
        depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        depthPassBeginInfo.renderPass = shadowMapRenderPass;
        depthPassBeginInfo.framebuffer = directionalShadowMapFramebuffer->GetVKFramebuffer();
        depthPassBeginInfo.renderArea.offset = { 0, 0 };
        depthPassBeginInfo.renderArea.extent.width = directionalShadowMapFramebuffer->GetWidth();
        depthPassBeginInfo.renderArea.extent.height = directionalShadowMapFramebuffer->GetHeight();
        depthPassBeginInfo.clearValueCount = 1;
        depthPassBeginInfo.pClearValues = &depthPassClearValue;


        // HDR pass begin Info.
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        HDRRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        HDRRenderPassBeginInfo.framebuffer = HDRFramebuffer->GetVKFramebuffer();
        HDRRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        HDRRenderPassBeginInfo.pClearValues = clearValues.data();
        HDRRenderPassBeginInfo.pNext = nullptr;
        HDRRenderPassBeginInfo.renderPass = HDRRenderPass;
        HDRRenderPassBeginInfo.renderArea.offset = { 0, 0 };
        HDRRenderPassBeginInfo.renderArea.extent.height = HDRFramebuffer->GetHeight();
        HDRRenderPassBeginInfo.renderArea.extent.width = HDRFramebuffer->GetWidth();



        CommandBuffer::CreateCommandBufferPool(s_GraphicsQueueFamily, cmdPool); 

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CommandBuffer::CreateCommandBuffer(cmdBuffers[i], cmdPool);
        }

        bloomAgent = std::make_shared<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage);

        finalPassSampler = Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::WriteDescriptorSetWithSampler(finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
    void OnUpdate() override
    {
        // Begin command buffer recording.
        CommandBuffer::BeginRecording(cmdBuffers[CurrentFrameIndex()]);

        // Timer.
        float deltaTime;
        deltaTime = DeltaTime();
        timer += 7.0f * deltaTime;

        // Update model matrices here.
        model2->Rotate(2.0f * deltaTime, 0, 1, 0);

        // Update the particle systems.
        fireSparks->UpdateParticles(deltaTime);
        fireSparks2->UpdateParticles(deltaTime);
        fireSparks3->UpdateParticles(deltaTime);
        fireSparks4->UpdateParticles(deltaTime);
        fireBase->UpdateParticles(deltaTime);
        fireBase2->UpdateParticles(deltaTime);
        fireBase3->UpdateParticles(deltaTime);
        fireBase4->UpdateParticles(deltaTime);
        ambientParticles->UpdateParticles(deltaTime);

        // Animating the directional light
        glm::mat4 directionalLightMVP = directionalLightProjectionMatrix * glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // General data.
        glm::mat4 cameraView = s_Camera->GetViewMatrix();
        glm::mat4 cameraProj = s_Camera->GetProjectionMatrix();
        glm::vec4 cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);
        glm::mat4 mat = model->GetTransform();
        glm::mat4 mat2 = model2->GetTransform();

        // Update some of parts of the global UBO buffer
        globalParametersUBO.viewMatrix = cameraView;
        globalParametersUBO.projMatrix = cameraProj;
        globalParametersUBO.cameraPosition = cameraPos;
        globalParametersUBO.dirLightPos = directionalLightPosition;
        globalParametersUBO.directionalLightMVP = directionalLightMVP;
        globalParametersUBO.viewportDimension = glm::vec4(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height, 0.0f, 0.0f);

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

        lightFlickerRate -= deltaTime * 1.0f;

        if (lightFlickerRate <= 0.0f)
        {
            lightFlickerRate = 0.1f;
            std::random_device rd;
            std::mt19937 gen(rd()); 
            std::uniform_real_distribution<> distr(25.0f, 50.0f); 
            std::uniform_real_distribution<> distr2(75.0f, 100.0f);
            std::uniform_real_distribution<> distr3(12.5f, 25.0f); 
            std::uniform_real_distribution<> distr4(25.0f, 50.0f); 
            globalParametersUBO.pointLightIntensities[0] = glm::vec4(distr(gen) / 2) ;
            globalParametersUBO.pointLightIntensities[1] = glm::vec4(distr2(gen) / 2);
            globalParametersUBO.pointLightIntensities[2] = glm::vec4(distr3(gen) / 2);
            globalParametersUBO.pointLightIntensities[3] = glm::vec4(distr4(gen) / 2);
            globalParametersUBO.pointLightIntensities[4] = glm::vec4(500.0f);
        }

        if (globalParametersUBO.enablePointLightShadows.x == 1.0f)
        {
            // Shadow passes ---------
            DirectionalShadowPass();
            PointLightShadowPass();
            // Shadow passes end  ----
        }

        // Copy the global UBO data from CPU to GPU. 
        memcpy(mappedGlobalParametersModelUBOBuffer, &globalParametersUBO, sizeof(GlobalParametersUBO));


        // TO DO: The animation sprite sheet offsets are hardcoded here. We could use a better system to automatically calculate these variables.
        aniamtionRate -= deltaTime * 1.0f;
        if (aniamtionRate <= 0)
        {
            aniamtionRate = 0.01388888f;
            currentAnimationFrame++;
            if (currentAnimationFrame > 72)
            {
                currentAnimationFrame = 0;
            }
        }

        int ct = 0;
        bool done = false;
        for (int i = 1; i <= 6; i++)
        {
            if (done)
                break;
        
            for (int j = 1; j <= 12; j++)
            {
                if (ct >= currentAnimationFrame)
                {
                    fireBase->RowOffset = 0.0833333333333333333333f * j;
                    fireBase->ColumnOffset = 0.166666666666666f * i;

                    fireBase2->RowOffset = 0.0833333333333333333333f * j;
                    fireBase2->ColumnOffset = 0.166666666666666f * i;

                    fireBase3->RowOffset = 0.0833333333333333333333f * j;
                    fireBase3->ColumnOffset = 0.166666666666666f * i;

                    fireBase4->RowOffset = 0.0833333333333333333333f * j;
                    fireBase4->ColumnOffset = 0.166666666666666f * i;
                    done = true;
                    break;
                }
                ct++;
            }
            ct++;
        }
        // Begin HDR rendering------------------------------------------
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], HDRRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Drawing the skybox.
        glm::mat4 skyBoxView = glm::mat4(glm::mat3(cameraView));
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], skyboxPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
        skybox->Draw(cmdBuffers[CurrentFrameIndex()], skyboxPipeline->GetPipelineLayout());

        struct pushConst
        {
            glm::mat4 modelMat;
            glm::vec4 color;
        };

        pushConst lightCubePC;
        // Drawing the light cube.
        glm::mat4 lightCubeMat = glm::mat4(1.0f);
        lightCubeMat = glm::translate(lightCubeMat, glm::vec3(globalParametersUBO.pointLightPositions[4].x, globalParametersUBO.pointLightPositions[4].y, globalParametersUBO.pointLightPositions[4].z));
        lightCubeMat = glm::scale(lightCubeMat, glm::vec3(0.05f));
        lightCubePC.modelMat = lightCubeMat;
        lightCubePC.color = glm::vec4(4.5f, 1.0f, 1.0f, 1.0f);
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &lightCubePC);
        cube->Draw(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout());

        // Drawing the Sponza.
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
        model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());

        // Drawing the helmet.
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
        model2->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());


        // Drawing 4 torches.
        for (int i = 0; i < torch->GetMeshCount(); i++)
        {
            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch1modelMatrix);
            torch->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());

            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch2modelMatrix);
            torch->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());

            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch3modelMatrix);
            torch->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());

            CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch4modelMatrix);
            torch->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());
        }

        pushConst swordPC;
        // Draw the emissive sword.
        swordPC.modelMat = model3->GetTransform();
        swordPC.color = glm::vec4(0.1f, 3.0f, 0.1f, 1.0f);
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &swordPC);
        model3->DrawIndexed(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout());


        // Draw the particles systems.
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline);

        glm::vec4 sparkBrigtness;
        sparkBrigtness.x = 5.0f;
        glm::vec4 flameBrigthness;
        flameBrigthness.x = 4.0f;
        glm::vec4 dustBrigthness;
        dustBrigthness.x = 1.0f;

        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &sparkBrigtness);
        fireSparks->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireSparks2->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireSparks3->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireSparks4->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());


        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &flameBrigthness);
        fireBase->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireBase2->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireBase3->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());
        fireBase4->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());

        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec4), &dustBrigthness);
        ambientParticles->Draw(cmdBuffers[CurrentFrameIndex()], particleSystemPipeline->GetPipelineLayout());


        //vkCmdEndRenderingKHR(cmdBuffers[CurrentFrameIndex()]);

        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
        // End HDR Rendering ------------------------------------------

        // Post processing begin ---------------------------
        bloomAgent->ApplyBloom(cmdBuffers[CurrentFrameIndex()]);
        // Post processing end ---------------------------

        // Final scene pass begin Info.
        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetActiveFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        finalScenePassBeginInfo.pClearValues = clearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[GetActiveImageIndex()]->GetVKFramebuffer();
        
        
        // Start final scene render pass (to swapchain).-------------------------------
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline);
        vkCmdBindDescriptorSets(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline->GetPipelineLayout(), 0, 1, &finalPassDescriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffers[CurrentFrameIndex()], 3, 1, 0, 0);
        
        
        //ImGui::ShowDemoWindow();
        
        ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.
        
        ImGui::DragFloat3("Directional Light", &directionalLightPosition.x, 0.1f, - 50, 50);
        
        float* p[3] = 
        {
            &model2->GetTransform()[3].x,
            &model2->GetTransform()[3].y,
            &model2->GetTransform()[3].z,
        };
        
        ImGui::DragFloat3("Helmet", *p, 0.01f, -10, 10);
        
        float* p2[3] =
        {
            &model3->GetTransform()[3].x,
            &model3->GetTransform()[3].y,
            &model3->GetTransform()[3].z,
        };
        
        ImGui::DragFloat3("Sword", *p2, 0.01f, -10, 10);
        
        float* t[3] =
        {
            &torch1modelMatrix[3].x,
            &torch1modelMatrix[3].y,
            &torch1modelMatrix[3].z,
        };
        
        ImGui::DragFloat3("Torch 1", *t, 0.01f, -10, 10);
        
        float* t2[3] =
        {
            &torch2modelMatrix[3].x,
            &torch2modelMatrix[3].y,
            &torch2modelMatrix[3].z,
        };
        
        ImGui::DragFloat3("Torch 2", *t2, 0.01f, -10, 10);
        
        float* t3[3] =
        {
            &torch3modelMatrix[3].x,
            &torch3modelMatrix[3].y,
            &torch3modelMatrix[3].z,
        };
        
        ImGui::DragFloat3("Torch 3", *t3, 0.01f, -10, 10);
        
        float* t4[3] =
        {
            &torch4modelMatrix[3].x,
            &torch4modelMatrix[3].y,
            &torch4modelMatrix[3].z,
        };
        
        ImGui::DragFloat3("Torch 4", *t4, 0.01f, -10, 10);
        
        float* p3[3] =
        {
            &globalParametersUBO.pointLightPositions[4].x,
            &globalParametersUBO.pointLightPositions[4].y,
            &globalParametersUBO.pointLightPositions[4].z
        };
        
        ImGui::DragFloat3("point light", *p3, 0.01f, -10, 10);
        
        if (ImGui::Checkbox("Point light shadows", &pointLightShadows))
        {
            pointLightShadows ? globalParametersUBO.enablePointLightShadows.x = 1.0f : globalParametersUBO.enablePointLightShadows.x = 0.0f;
        }
        
        
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
        ImGui::End();
        
        ImGui::Render();
        
        ImDrawData* draw_data = ImGui::GetDrawData();
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmdBuffers[CurrentFrameIndex()]);
        
        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
        // End the command buffer recording phase(swapchain).-------------------------------.

        CommandBuffer::EndRecording(cmdBuffers[CurrentFrameIndex()]);

        SubmitCommandBuffer(cmdBuffers[CurrentFrameIndex()]);
    }
    
    void OnCleanup() override
    {
        delete model;
        delete model2;
        delete model3;
        delete skybox;
        delete cube;
        delete torch;
        delete fireSparks;
        delete fireBase;
        delete fireSparks2;
        delete fireBase2;
        delete fireSparks3;
        delete fireBase3;
        delete fireSparks4;
        delete fireBase4;
        delete ambientParticles;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CommandBuffer::FreeCommandBuffer(cmdBuffers[i], cmdPool, s_Device->GetGraphicsQueue());
        }
        CommandBuffer::DestroyCommandPool(cmdPool);
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), finalPassSampler, nullptr);
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), shadowMapRenderPass, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBufferMemory, nullptr);
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBuffer, nullptr);
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), HDRRenderPass, nullptr);
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), pointShadowRenderPass, nullptr);

    }
    void OnWindowResize() override
    {
        pipeline->ReConstruct();
        shadowPassPipeline->ReConstruct();
        cubePipeline->ReConstruct();
        skyboxPipeline->ReConstruct();
        particleSystemPipeline->ReConstruct();
        finalPassPipeline->ReConstruct();
        EmissiveObjectPipeline->ReConstruct();

        CreateHDRFramebuffer();

        bloomAgent = std::make_shared<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloomTo(HDRColorImage);

        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), finalPassSampler, nullptr);

        finalPassSampler = Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::WriteDescriptorSetWithSampler(finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Reassign new framebuffer dimensions to the render pass begin infos.
        HDRRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        HDRRenderPassBeginInfo.framebuffer = HDRFramebuffer->GetVKFramebuffer();
        HDRRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        HDRRenderPassBeginInfo.pClearValues = clearValues.data();
        HDRRenderPassBeginInfo.pNext = nullptr;
        HDRRenderPassBeginInfo.renderPass = HDRRenderPass;
        HDRRenderPassBeginInfo.renderArea.offset = { 0, 0 };
        HDRRenderPassBeginInfo.renderArea.extent.height = HDRFramebuffer->GetHeight();
        HDRRenderPassBeginInfo.renderArea.extent.width = HDRFramebuffer->GetWidth();

        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetActiveFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        finalScenePassBeginInfo.pClearValues = clearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[GetActiveImageIndex()]->GetVKFramebuffer();
    }
    float DeltaTime()
    {
        float currentFrameRenderTime, deltaTime;
        currentFrameRenderTime = GetRenderTime();
        deltaTime = currentFrameRenderTime - m_DeltaTimeLastFrame;
        m_DeltaTimeLastFrame = currentFrameRenderTime;
        return deltaTime;
    }
};
int main()
{
	MyApplication app(MAX_FRAMES_IN_FLIGHT);
	app.Run();
	return 0;
}