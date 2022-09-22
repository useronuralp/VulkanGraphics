#include "VulkanApplication.h"
#include "Model.h"
#include "Image.h"
#include "Framebuffer.h"
#include "Pipeline.h"
#include "Surface.h"
#include "Swapchain.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "PhysicalDevice.h"
#include "CommandBuffer.h"
#include "Window.h"
#include "CommandBuffer.h"
#include "Camera.h"
#include "Buffer.h"
#include "Mesh.h"
#include "ParticleSystem.h"

#include <simplexnoise.h>
#include <random>
#include <Curl.h>

#define POINT_LIGHT_COUNT 4
#define SHADOW_DIM 10000 // Shadow map resolution.
#define MAX_FRAMES_IN_FLIGHT 2 // TO DO: I can't seem to get an FPS boost by rendering multiple frames at once.
int CURRENT_FRAME = 0;

using namespace OVK;
class MyApplication : public OVK::VulkanApplication
{
    struct ModelUBO
    {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::mat4 depthMVP;
        glm::vec4 dirLightPos;
        glm::vec4 cameraPosition;
        glm::vec4 viewportDimension;
    };

    struct LightPropertiesUBO
    {
        glm::vec4 lightPositions[POINT_LIGHT_COUNT];
        glm::vec4 lightIntensities[POINT_LIGHT_COUNT];;
    };

public:
    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen; // seed the generator
    std::uniform_real_distribution<> distr;

#pragma region Layouts
    // Layouts
    Ref<DescriptorLayout> setLayout;
    Ref<DescriptorLayout> setLayout2;
    Ref<DescriptorLayout> particleSystemLayout;
#pragma endregion

#pragma region Pools
    // Pools
    Ref<DescriptorPool> pool;
    Ref<DescriptorPool> pool2;
    Ref<DescriptorPool> particleSystemPool;
#pragma endregion

#pragma region Pipelines
    // Pipelines
    Ref<Pipeline> pipeline;
    Ref<Pipeline> shadowPassPipeline;
    Ref<Pipeline> skyboxPipeline;
    Ref<Pipeline> particleSystemPipeline;
#pragma endregion

#pragma region ModelsANDParticleSystems
    // Models
    Model* model;
    Model* model2;
    Model* torch;
    Model* skybox;

    ParticleSystem* fireSparks;
    ParticleSystem* fireBase;

    ParticleSystem* fireSparks2;
    ParticleSystem* fireBase2;

    ParticleSystem* fireSparks3;
    ParticleSystem* fireBase3;

    ParticleSystem* fireSparks4;
    ParticleSystem* fireBase4;
#pragma endregion

#pragma region UBOs
    ModelUBO                modelUBO;
    VkBuffer                modelUBOBuffer;
    VkDeviceMemory          modelUBOBufferMemory;
    void*                   mappedModelUBOBuffer;

    LightPropertiesUBO      lightUBO;
    VkBuffer                lightUBOBuffer;
    VkDeviceMemory          lightUBOBufferMemory;
    void*                   mappedLightUBOBuffer;
#pragma endregion

    glm::mat4 torch1modelMatrix {1.0};
    glm::mat4 torch2modelMatrix {1.0};
    glm::mat4 torch3modelMatrix {1.0};
    glm::mat4 torch4modelMatrix {1.0};

    Ref<Texture>    particleTexture;
    Ref<Texture>    fireTexture;


    float lightFlickerRate = 0.07f;
    float aniamtionRate = 0.013888888f;
    int currentAnimationFrame = 0;

    float  timer = 0.0f;

    glm::vec4 pointLightPositions[POINT_LIGHT_COUNT];
    glm::vec4 pointLightIntensities[POINT_LIGHT_COUNT];
    glm::vec4 directionalLightPosition = glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f);

    float near_plane = 5.0f;
    float far_plane = 200.0f;
    int   frameCount = 0;

    glm::mat4 lightViewMatrix       = glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, near_plane, far_plane);
    glm::mat4 lightModelMatrix      = glm::mat4(1.0f);
    glm::mat4 depthMVP              = lightProjectionMatrix * lightViewMatrix * lightModelMatrix;

    VkClearValue                depthPassClearValue;
    std::array<VkClearValue, 2> scenePassClearValues{};

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame = GetRenderTime();

    Ref<Texture>     shadowMapTexture;
    VkRenderPass     shadowMapRenderPass;
    Ref<Framebuffer> shadowMapFramebuffer;

    VkRenderPassBeginInfo depthPassBeginInfo;
    VkRenderPassBeginInfo finalScenePassBeginInfo;

    VkCommandBuffer cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool cmdPools[MAX_FRAMES_IN_FLIGHT];
    
    // Rendering semaphores
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderingCompleteSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence     inRenderingFences[MAX_FRAMES_IN_FLIGHT];

    VkCommandBuffer commandBuffer;
    VkCommandPool   commandPool;

    // Helpers
    void WriteDescriptorSetWithUBOs(Model* model)
    {
        for (int i = 0; i < model->GetMeshes().size(); i++)
        {
            // Write the descriptor set.
            VkWriteDescriptorSet descriptorWrite{};
            VkDescriptorBufferInfo bufferInfo{};

            bufferInfo.buffer = modelUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(ModelUBO);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = model->GetMeshes()[i]->GetDescriptorSet();
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            model->GetMeshes()[i]->WriteDescriptorSet(descriptorWrite);

            bufferInfo.buffer = lightUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(LightPropertiesUBO);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = model->GetMeshes()[i]->GetDescriptorSet();
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            model->GetMeshes()[i]->WriteDescriptorSet(descriptorWrite);         
        }
    }
    void WriteDescriptorSetSkybox(Model* model)
    {
        // Write the descriptor set.
        VkWriteDescriptorSet descriptorWrite{};
        VkDescriptorBufferInfo bufferInfo{};

        bufferInfo.buffer = modelUBOBuffer;
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

        model->GetMeshes()[0]->WriteDescriptorSet(descriptorWrite);
    }

    void SetupScenePassPipeline()
    {
        // Configure the pipeline spcifications. All of these fields except the "DescriptorSetLayout" is mandatory.
        Pipeline::Specs specs{};
        specs.DescriptorLayout = setLayout;
        specs.pRenderPass = &VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
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
        specs.EnablePushConstant = true;
        specs.PushConstantOffset = 0;
        specs.PushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT;
        specs.PushConstantSize = sizeof(glm::mat4);

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

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
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
        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;
        pipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupShadowPassPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = setLayout;
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
        specs.FragmentShaderPath = "shaders/shadowPassFRAG.spv";
        specs.ViewportHeight = SHADOW_DIM;
        specs.ViewportWidth = SHADOW_DIM;
        specs.EnablePushConstant = true;
        specs.PushConstantOffset = 0;
        specs.PushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT;
        specs.PushConstantSize = sizeof(glm::mat4);

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


        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        shadowPassPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupSkyboxPipeline()
    {
        Pipeline::Specs specs{};
        specs.DescriptorLayout = setLayout2;
        specs.pRenderPass = &VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
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
        specs.EnablePushConstant = true;
        specs.PushConstantOffset = 0;
        specs.PushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT;
        specs.PushConstantSize = sizeof(glm::mat4);

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

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);
        // For position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        skyboxPipeline = std::make_shared<Pipeline>(specs);
    }
    void SetupParticleSystemPipeline()
    {
        Pipeline::Specs particleSpecs{};
        Ref<DescriptorLayout> layout = particleSystemLayout;
        particleSpecs.DescriptorLayout = layout;
        particleSpecs.pRenderPass = &VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
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

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Particle);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(9);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Particle, Position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Particle, Color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Particle, Alpha);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Particle, SizeRadius);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Particle, Rotation);

        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[5].offset = offsetof(Particle, RowOffset);

        attributeDescriptions[6].binding = 0;
        attributeDescriptions[6].location = 6;
        attributeDescriptions[6].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[6].offset = offsetof(Particle, ColumnOffset);

        attributeDescriptions[7].binding = 0;
        attributeDescriptions[7].location = 7;
        attributeDescriptions[7].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[7].offset = offsetof(Particle, RowCellSize);

        attributeDescriptions[8].binding = 0;
        attributeDescriptions[8].location = 8;
        attributeDescriptions[8].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[8].offset = offsetof(Particle, ColumnCellSize);

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);
        particleSpecs.pVertexBindingDesc = bindingDescs;
        particleSpecs.pVertexAttributeDescriptons = attributeDescriptions;

        particleSystemPipeline = std::make_shared<Pipeline>(particleSpecs);
    }

    void SetupLightSphere(Model* model)
    {
        std::vector<DescriptorBindingSpecs> dscLayout
        { 
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,sizeof(glm::mat4), 1, ShaderStage::VERTEX , 0},
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,sizeof(glm::mat4), 1, ShaderStage::VERTEX , 1},
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,sizeof(glm::mat4), 1, ShaderStage::VERTEX , 2},
        };

        Pipeline::Specs specs{};
        specs.DescriptorLayout = VK_NULL_HANDLE;
        specs.pRenderPass = &VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
        specs.CullMode = VK_CULL_MODE_BACK_BIT;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/lightsourceVERT.spv";
        specs.FragmentShaderPath = "shaders/lightsourceFRAG.spv";

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


        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);
        // For position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        //model->AddConfiguration("NormalRenderPass", specs, dscLayout);
        //model->SetActiveConfiguration("NormalRenderPass");
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

        ASSERT(vkCreateRenderPass(s_Device->GetVKDevice(), &renderPassInfo, nullptr, &shadowMapRenderPass) == VK_SUCCESS, "Failed to create a render pass.");
    }
    void SetupParticleSystems()
    {
        particleTexture = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "OVKLib/textures/spark.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);
        fireTexture = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "OVKLib/textures/fire_sprite_sheet.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);

        ParticleSpecs specs{};
        specs.ParticleCount = 10;
        specs.EnableNoise = true;
        specs.TrailLength = 2;
        specs.SphereRadius = 0.05f;
        specs.ImmortalParticle = false;
        specs.ParticleSize = 0.5f;
        specs.EmitterPos = glm::vec3(2.45f, 1.55f, 0.8f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks = new ParticleSystem(specs, particleTexture, particleSystemLayout, particleSystemPool);
        fireSparks->SetUBO(modelUBOBuffer, sizeof(ModelUBO));

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(2.45f, 1.6f, -1.14f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase = new ParticleSystem(specs , fireTexture, particleSystemLayout, particleSystemPool);
        fireBase->SetUBO(modelUBOBuffer, sizeof(ModelUBO));
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
        specs.EmitterPos = glm::vec3(2.45f, 1.55f, -1.15f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks2 = new ParticleSystem(specs, particleTexture, particleSystemLayout, particleSystemPool);
        fireSparks2->SetUBO(modelUBOBuffer, sizeof(ModelUBO));

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(2.45f, 1.6f, 0.785f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase2 = new ParticleSystem(specs, fireTexture, particleSystemLayout, particleSystemPool);
        fireBase2->SetUBO(modelUBOBuffer, sizeof(ModelUBO));
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
        specs.EmitterPos = glm::vec3(0.6f, 1.55f, 0.8f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks3 = new ParticleSystem(specs, particleTexture, particleSystemLayout, particleSystemPool);
        fireSparks3->SetUBO(modelUBOBuffer, sizeof(ModelUBO));

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(0.6f, 1.6f, 0.785f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase3 = new ParticleSystem(specs, fireTexture, particleSystemLayout, particleSystemPool);
        fireBase3->SetUBO(modelUBOBuffer, sizeof(ModelUBO));
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
        specs.EmitterPos = glm::vec3(0.6f, 1.55f, -1.15f);
        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 3.0f;
        specs.MinVel = glm::vec3(-1.0f, 0.1f, -1.0f);
        specs.MaxVel = glm::vec3(1.0f, 2.0f, 1.0f);

        fireSparks4 = new ParticleSystem(specs, particleTexture, particleSystemLayout, particleSystemPool);
        fireSparks4->SetUBO(modelUBOBuffer, sizeof(ModelUBO));

        specs.ParticleMinLifetime = 0.1f;
        specs.ParticleMaxLifetime = 1.5f;
        specs.SphereRadius = 0.0f;
        specs.TrailLength = 0;
        specs.ParticleCount = 1;
        specs.ImmortalParticle = true;
        specs.ParticleSize = 13.0f;
        specs.EnableNoise = false;
        specs.EmitterPos = glm::vec3(0.6f, 1.6f, -1.15f);
        specs.MinVel = glm::vec4(0.0f);
        specs.MaxVel = glm::vec4(0.0f);

        fireBase4 = new ParticleSystem(specs, fireTexture, particleSystemLayout, particleSystemPool);
        fireBase4->SetUBO(modelUBOBuffer, sizeof(ModelUBO));
        fireBase4->RowOffset = 0.0f;
        fireBase4->RowCellSize = 0.0833333333333333333333f;
        fireBase4->ColumnCellSize = 0.166666666666666f;
        fireBase4->ColumnOffset = 0.0f;
    }
private:
    void OnVulkanInit()
    {
        // Set the device extensions we want Vulkan to enable.
        SetDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME });

        // Set the parameters of the main camera.
        SetCameraConfiguration(45.0f, 0.1f, 1500.0f);
    }

    void OnStart()
    {
        CurlNoise::SetCurlSettings(false, 4.0f, 6, 1.0, 0.0);

        std::vector<DescriptorBindingSpecs> dscLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(ModelUBO),               1, ShaderStage::VERTEX,    0},
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(LightPropertiesUBO),     1, ShaderStage::FRAGMENT,  1},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                     1, ShaderStage::FRAGMENT , 2},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_NORMAL,             UINT64_MAX,                     1, ShaderStage::FRAGMENT , 3},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC,  UINT64_MAX,                     1, ShaderStage::FRAGMENT , 4},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_SHADOWMAP,          UINT64_MAX,                     1, ShaderStage::FRAGMENT , 5},
        };

        std::vector<DescriptorBindingSpecs> dscLayout2
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER, sizeof(glm::mat4), 1, ShaderStage::VERTEX , 0},  // Index 0
        };

        std::vector<DescriptorBindingSpecs> dscLayout3
        {
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,           sizeof(glm::mat4),  1, ShaderStage::VERTEX  , 0},
            DescriptorBindingSpecs {Type::TEXTURE_SAMPLER_CUBEMAP,  UINT64_MAX,         1, ShaderStage::FRAGMENT, 1}
        };

        std::vector<DescriptorBindingSpecs> dscLayout4
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,     sizeof(ModelUBO),             1, ShaderStage::VERTEX   , 0}, // Index 0
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,  UINT64_MAX,            1, ShaderStage::FRAGMENT , 1}, // Index 3
        };

        std::vector<VkDescriptorType> types;
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        setLayout = std::make_shared<DescriptorLayout>(dscLayout);
        pool = std::make_shared<DescriptorPool>(200, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        setLayout2 = std::make_shared<DescriptorLayout>(dscLayout3);
        pool2 = std::make_shared<DescriptorPool>(100, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        particleSystemLayout = std::make_shared<DescriptorLayout>(dscLayout4);
        particleSystemPool = std::make_shared<DescriptorPool>(100, types);



        // Following are the global Uniform Buffes shared by all shaders.
        Utils::CreateVKBuffer(sizeof(ModelUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, modelUBOBuffer, modelUBOBufferMemory);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), modelUBOBufferMemory, 0, sizeof(ModelUBO), 0, &mappedModelUBOBuffer);
        
        Utils::CreateVKBuffer(sizeof(LightPropertiesUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, lightUBOBuffer, lightUBOBufferMemory);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), lightUBOBufferMemory, 0, sizeof(LightPropertiesUBO), 0, &mappedLightUBOBuffer);

        // Set the positions of the point lights in the scene we have 4 torches.
        lightUBO.lightPositions[0] = glm::vec4(2.46f, 1.54f, 0.76f, 1.0f);
        lightUBO.lightPositions[1] = glm::vec4(0.62f, 1.54f, 0.76f, 1.0f);
        lightUBO.lightPositions[2] = glm::vec4(0.62f, 1.54f, -1.16f, 1.0f);
        lightUBO.lightPositions[3] = glm::vec4(2.46f, 1.54f, -1.16f, 1.0f);

        SetupParticleSystems();

        // Create a texture for the shadowmap. We will render to this texture when we are doing a shadow pass.
        shadowMapTexture = std::make_shared<Texture>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, ImageType::DEPTH);

        // Creating our shadow pass with this helper function.
        CreateShadowRenderPass();

        // Create the pipelines for different works.
        SetupScenePassPipeline();
        SetupShadowPassPipeline();
        SetupSkyboxPipeline();
        SetupParticleSystemPipeline();

        // Final piece of the puzzle is the framebuffer. We need a framebuffer to link the image we are rendering to with the render pass.
        shadowMapFramebuffer = std::make_shared<Framebuffer>(shadowMapRenderPass, shadowMapTexture);
        
        // Loading the model Sponza
        model = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\Sponza\\scene.gltf", LOAD_VERTICES | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, setLayout, shadowMapTexture);
        model->Scale(0.005f, 0.005f, 0.005f);
        WriteDescriptorSetWithUBOs(model);

        // Loading the model Malenia's Helmet.
        model2 = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\MaleniaHelmet\\scene.gltf", LOAD_VERTICES | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, setLayout, shadowMapTexture);
        model2->Scale(0.7f, 0.7f, 0.7f);
        model2->Translate(2.0f, 2.0f, -0.2f);
        model2->Rotate(90, 0, 1, 0);
        WriteDescriptorSetWithUBOs(model2);

        // Loading 4 torches. TO DO: This part should be converted to instanced drawing. There are unnecessary data duplications happening here.
        torch = new Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\torch\\scene.gltf", LOAD_VERTICES | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, setLayout, shadowMapTexture);
        torch1modelMatrix = glm::scale(torch1modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
        torch1modelMatrix = glm::rotate(torch1modelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
        torch1modelMatrix = glm::translate(torch1modelMatrix, glm::vec3(-2.7f, 4.3f, 8.2f));

        torch2modelMatrix = glm::scale(torch2modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
        torch2modelMatrix = glm::rotate(torch2modelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));
        torch2modelMatrix = glm::translate(torch2modelMatrix, glm::vec3(-3.88f, 4.3f, -8.2f));

        torch3modelMatrix = glm::scale(torch3modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
        torch3modelMatrix = glm::rotate(torch3modelMatrix, glm::radians(90.0f), glm::vec3(0, 1, 0));
        torch3modelMatrix = glm::translate(torch3modelMatrix, glm::vec3(-2.7f, 4.3f, 2.0f));

        torch4modelMatrix = glm::scale(torch4modelMatrix, glm::vec3(0.3f, 0.3f, 0.3f));
        torch4modelMatrix = glm::rotate(torch4modelMatrix, glm::radians(-90.0f), glm::vec3(0, 1, 0));
        torch4modelMatrix = glm::translate(torch4modelMatrix, glm::vec3(-3.88f, 4.3f, -2.0f));
        WriteDescriptorSetWithUBOs(torch);


        // Vertex data for the skybox.
        const float cubeVertices[3 * 6 * 6] = {
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
        std::string front   = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\front.png");
        std::string back    = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\back.png");
        std::string top     = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\top.png");
        std::string bottom  = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\bottom.png");
        std::string right   = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\right.png");
        std::string left    = (std::string(SOLUTION_DIR) + "OVKLib\\textures\\skybox\\Starry\\left.png");
        
        // Set up the 6 sided texture for the skybox by using the above images.
        std::array<std::string, 6> skyboxTex { right, left, top, bottom, front, back};
        Ref<OVK::CubemapTexture> cubemap = std::make_shared<CubemapTexture>(skyboxTex, VK_FORMAT_R8G8B8A8_SRGB);
        
        // Create the mesh for the skybox.
        skybox = new Model(cubeVertices, (size_t)(sizeof(float) * 3 * 6 * 6), 6 * 6, cubemap, pool2, setLayout2);
        WriteDescriptorSetSkybox(skybox);

        // Configure the render pass begin info for the depth pass here.
        depthPassClearValue = { 1.0f, 0.0 };
        
        depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        depthPassBeginInfo.renderPass = shadowMapRenderPass;
        depthPassBeginInfo.framebuffer = shadowMapFramebuffer->GetVKFramebuffer();
        depthPassBeginInfo.renderArea.offset = { 0, 0 };
        depthPassBeginInfo.renderArea.extent.width = shadowMapFramebuffer->GetWidth();
        depthPassBeginInfo.renderArea.extent.height = shadowMapFramebuffer->GetHeight();
        depthPassBeginInfo.clearValueCount = 1;
        depthPassBeginInfo.pClearValues = &depthPassClearValue;

        // Setup the fences and semaphores needed to synchronize the rendering.
        VkFenceCreateInfo fenceCreateInfo = {};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CommandBuffer::Create(s_GraphicsQueueFamily, cmdPools[i], cmdBuffers[i]);
            ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &renderingCompleteSemaphores[i]) == VK_SUCCESS, "Failed to create rendering complete semaphore.");
            ASSERT(vkCreateFence(s_Device->GetVKDevice(), &fenceCreateInfo, nullptr, &inRenderingFences[i]) == VK_SUCCESS, "Failed to create is rendering fence.");
            ASSERT(vkCreateSemaphore(s_Device->GetVKDevice(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) == VK_SUCCESS, "Failed to create image available semaphore.");
        }
        CommandBuffer::Create(s_GraphicsQueueFamily, commandPool, commandBuffer);
	}
    void OnUpdate()
    {
        vkWaitForFences(s_Device->GetVKDevice(), 1, &inRenderingFences[CURRENT_FRAME], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = -1;
        VkResult rslt = vkAcquireNextImageKHR(s_Device->GetVKDevice(), s_Swapchain->GetVKSwapchain(), UINT64_MAX, imageAvailableSemaphores[CURRENT_FRAME], VK_NULL_HANDLE, &imageIndex);

        // Return and start a new frame if the screen is resized.
        if (rslt == VK_ERROR_OUT_OF_DATE_KHR || s_Window->IsWindowResized())
        {
            vkDeviceWaitIdle(s_Device->GetVKDevice());
            s_Swapchain->OnResize();
            OnWindowResize();
            s_Window->OnResize();
            s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
            return;
        }
        ASSERT(rslt == VK_SUCCESS, "Failed to acquire next image.");


        vkResetFences(s_Device->GetVKDevice(), 1, &inRenderingFences[CURRENT_FRAME]);

        // Begin command buffer recording.
        CommandBuffer::Begin(cmdBuffers[CURRENT_FRAME]);

        // Timer.
        float deltaTime;
        deltaTime = DeltaTime();
        timer += 7.0f * deltaTime;

        // Animating the light
        //directionalLightPosition.z = std::sin(glm::radians(timer)) * 9.0f;
        lightViewMatrix = glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        depthMVP = lightProjectionMatrix * lightViewMatrix * lightModelMatrix;


        lightFlickerRate -= deltaTime * 1.0f;

        if (lightFlickerRate <= 0.0f)
        {
            lightFlickerRate = 0.1f;
            std::random_device rd;
            std::mt19937 gen(rd()); 
            std::uniform_real_distribution<> distr(10.0f, 20.0f); 
            std::uniform_real_distribution<> distr2(30.0f, 40.0f);
            std::uniform_real_distribution<> distr3(5.0f, 10.0f); 
            std::uniform_real_distribution<> distr4(1.0f, 5.0f); 
            lightUBO.lightIntensities[0] = glm::vec4(distr(gen));
            lightUBO.lightIntensities[1] = glm::vec4(distr2(gen));
            lightUBO.lightIntensities[2] = glm::vec4(distr3(gen));
            lightUBO.lightIntensities[3] = glm::vec4(distr4(gen));
        }

        memcpy(mappedLightUBOBuffer, &lightUBO, sizeof(lightUBO));
        glm::mat4 view = s_Camera->GetViewMatrix();
        glm::mat4 proj = s_Camera->GetProjectionMatrix();
        glm::vec4 cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);

        modelUBO.viewMatrix = view;
        modelUBO.projMatrix = proj;
        modelUBO.cameraPosition = cameraPos;
        modelUBO.dirLightPos = directionalLightPosition;
        modelUBO.depthMVP = depthMVP;
        modelUBO.viewportDimension = glm::vec4(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height, 0.0f, 0.0f);
        memcpy(mappedModelUBOBuffer, &modelUBO, sizeof(modelUBO));
        
        glm::mat4 mat = model->GetModelMatrix();
        model2->Rotate(5.0f * deltaTime, 0.0, 1, 0.0f);
        glm::mat4 mat2 = model2->GetModelMatrix();

        VkDeviceSize offset = 0;

        // Start shadow pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CURRENT_FRAME], depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline->GetVKPipeline());
        // Render the objects you want to cast shadows.
        for (int i = 0; i < model->GetMeshes().size(); i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline->GetPipelineLayout(), 0, 1, &model->GetMeshes()[i]->GetDescriptorSet(), 0, nullptr);
            vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &model->GetMeshes()[i]->GetVBO()->GetVKBuffer(), &offset);
            vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], model->GetMeshes()[i]->GetIBO()->GetVKBuffer(), offset, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], model->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }
        
        for (int i = 0; i < model2->GetMeshes().size(); i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline->GetPipelineLayout(), 0, 1, &model2->GetMeshes()[i]->GetDescriptorSet(), 0, nullptr);
            vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &model2->GetMeshes()[i]->GetVBO()->GetVKBuffer(), &offset);
            vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], model2->GetMeshes()[i]->GetIBO()->GetVKBuffer(), offset, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], model2->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }
        
        // End shadow pass.
        CommandBuffer::EndRenderPass(cmdBuffers[CURRENT_FRAME]);

        // Setup the actual scene render pass here.
        scenePassClearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };
        scenePassClearValues[1].depthStencil = { 1.0f, 0 };

        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[imageIndex]->GetVKFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(scenePassClearValues.size());
        finalScenePassBeginInfo.pClearValues = scenePassClearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[imageIndex]->GetVKFramebuffer();


        
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

        // TO DO: The animation sprite sheet offsets are hardcoded here. We could use a better system to automatically calculate these variables.
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

        // Start final scene render pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CURRENT_FRAME], finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        glm::mat4 skyBoxView = glm::mat4(glm::mat3(view));
        //// Drawing the skybox.
        vkCmdBindPipeline(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline->GetVKPipeline());
        vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline->GetPipelineLayout(), 0, 1, &skybox->GetMeshes()[0]->GetDescriptorSet(), 0, nullptr);
        vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &skybox->GetMeshes()[0]->GetVBO()->GetVKBuffer(), &offset);
        vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
        vkCmdDraw(cmdBuffers[CURRENT_FRAME], 36, 1, 0, 0);


        vkCmdBindPipeline(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVKPipeline());
        // Drawing the Sponza.        
        for (int i = 0; i < model->GetMeshes().size(); i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &model->GetMeshes()[i]->GetDescriptorSet(), 0, nullptr);
            vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &model->GetMeshes()[i]->GetVBO()->GetVKBuffer(), &offset);
            vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], model->GetMeshes()[i]->GetIBO()->GetVKBuffer(), offset, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], model->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }

        // Drawing the helmet.
        for (int i = 0; i < model2->GetMeshes().size(); i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &model2->GetMeshes()[i]->GetDescriptorSet(), 0, nullptr);
            vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &model2->GetMeshes()[i]->GetVBO()->GetVKBuffer(), &offset);
            vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], model2->GetMeshes()[i]->GetIBO()->GetVKBuffer(), offset, VK_INDEX_TYPE_UINT32);
            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], model2->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }

        // Drawing 4 torches.
        for (int i = 0; i < torch->GetMeshes().size(); i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &torch->GetMeshes()[i]->GetDescriptorSet(), 0, nullptr);
            vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &torch->GetMeshes()[i]->GetVBO()->GetVKBuffer(), &offset);
            vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], torch->GetMeshes()[i]->GetIBO()->GetVKBuffer(), offset, VK_INDEX_TYPE_UINT32);

            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch1modelMatrix);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], torch->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);

            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch2modelMatrix);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], torch->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);

            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch3modelMatrix);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], torch->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);

            vkCmdPushConstants(cmdBuffers[CURRENT_FRAME], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &torch4modelMatrix);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], torch->GetMeshes()[i]->GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }

        // Update the particle systems.
        fireSparks->UpdateParticles(deltaTime);
        fireSparks2->UpdateParticles(deltaTime);
        fireSparks3->UpdateParticles(deltaTime);
        fireSparks4->UpdateParticles(deltaTime);
        fireBase->UpdateParticles(deltaTime);
        fireBase2->UpdateParticles(deltaTime);
        fireBase3->UpdateParticles(deltaTime);
        fireBase4->UpdateParticles(deltaTime);

        // Draw the particles systems.
        vkCmdBindPipeline(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline->GetVKPipeline());
        fireSparks->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        fireBase->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        
        fireSparks2->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        fireBase2->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        
        fireSparks3->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        fireBase3->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        
        fireSparks4->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());
        fireBase4->Draw(cmdBuffers[CURRENT_FRAME], particleSystemPipeline->GetPipelineLayout());


        // End the command buffer recording phase.
        CommandBuffer::EndRenderPass(cmdBuffers[CURRENT_FRAME]);
        CommandBuffer::End(cmdBuffers[CURRENT_FRAME]);

        // Submit the command buffer to a queue.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[CURRENT_FRAME]};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffers[CURRENT_FRAME];
        VkSemaphore signalSemaphores[] = { renderingCompleteSemaphores[CURRENT_FRAME]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;


        VkQueue graphicsQueue = s_Device->GetGraphicsQueue();
        ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inRenderingFences[CURRENT_FRAME]) == VK_SUCCESS, "Failed to submit draw command buffer!");

        // Present the drawn image to the swapchain when the drawing is completed. This check is done via a semaphore.
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { s_Swapchain->GetVKSwapchain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        VkQueue presentQueue = s_Device->GetPresentQueue();
        VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

        // Check if window has been resized.
        if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            vkDeviceWaitIdle(s_Device->GetVKDevice());
            s_Swapchain->OnResize();
            OnWindowResize();
            s_Camera->SetViewportSize(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);
            return;
        }
        ASSERT(result == VK_SUCCESS, "Failed to present swap chain image!");

        // Reset the command buffer so that we can record new command next frame.
        CommandBuffer::Reset(cmdBuffers[CURRENT_FRAME]);
        CURRENT_FRAME = ++CURRENT_FRAME % MAX_FRAMES_IN_FLIGHT;
    }
    void OnCleanup()
    {
        delete model;
        delete model2;
        delete skybox;
        delete torch;
        delete fireSparks;
        delete fireBase;
        delete fireSparks2;
        delete fireBase2;
        delete fireSparks3;
        delete fireBase3;
        delete fireSparks4;
        delete fireBase4;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(s_Device->GetVKDevice(), imageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(s_Device->GetVKDevice(), renderingCompleteSemaphores[i], nullptr);
            vkDestroyFence(s_Device->GetVKDevice(), inRenderingFences[i], nullptr);
            CommandBuffer::FreeCommandBuffer(cmdBuffers[i], cmdPools[i]);
        }

        CommandBuffer::FreeCommandBuffer(commandBuffer, commandPool);
    }
    void OnWindowResize()
    {
        pipeline->OnResize();
        shadowPassPipeline->OnResize();
        skyboxPipeline->OnResize();
        particleSystemPipeline->OnResize();
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
	MyApplication app;
	app.Run();
	return 0;
}