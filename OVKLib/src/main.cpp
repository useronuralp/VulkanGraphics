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
#include "Bloom.h"

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
public:
    MyApplication(uint32_t framesInFlight) : VulkanApplication(framesInFlight){}
private:
    struct GlobalParametersUBO
    {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
        glm::mat4 depthMVP;
        glm::vec4 dirLightPos;
        glm::vec4 cameraPosition;
        glm::vec4 viewportDimension;
    };

    struct GlobalLightPropertiesUBO
    {
        glm::vec4 lightPositions[POINT_LIGHT_COUNT];
        glm::vec4 lightIntensities[POINT_LIGHT_COUNT];
        glm::vec4 directionalLightIntensity;
    };

public:
    // HDR framebuffer variables.
    Ref<Framebuffer>            HDRFramebuffer;
    VkRenderPass                HDRRenderPass;
    Ref<Image>                  HDRColorImage;
    Ref<Image>                  HDRDepthImage;
    VkRenderPassBeginInfo       HDRRenderPassBeginInfo;
    std::array<VkClearValue, 2> clearValues;
    Ref<Pipeline>               EmissiveObjectPipeline;
    Ref<Bloom>                  bloomAgent;

    Ref<DescriptorLayout> oneSamplerLayout;
    Ref<DescriptorLayout> emissiveLayout;

    VkSampler               finalPassSampler;
    VkDescriptorSet         finalPassDescriptorSet;
    Ref<Pipeline>           finalPassPipeline;


    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen; // seed the generator
    std::uniform_real_distribution<> distr;

#pragma region Layouts
    // Layouts
    Ref<DescriptorLayout> PBRLayout;
    Ref<DescriptorLayout> skyboxLayout;
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
    Model* model3;
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
    GlobalParametersUBO         globalParametersUBO;
    VkBuffer                    globalParametersUBOBuffer;
    VkDeviceMemory              globalParametersUBOBufferMemory;
    void*                       globalParametersModelUBOBuffer;

    GlobalLightPropertiesUBO    globalLightParametersUBO;
    VkBuffer                    globalLightParametersUBOBuffer;
    VkDeviceMemory              globalLightParametersUBOBufferMemory;
    void*                       mappedGlobalLightParametersUBOBuffer;
#pragma endregion

    glm::mat4 torch1modelMatrix {1.0};
    glm::mat4 torch2modelMatrix {1.0};
    glm::mat4 torch3modelMatrix {1.0};
    glm::mat4 torch4modelMatrix {1.0};

    Ref<Image>    particleTexture;
    Ref<Image>    fireTexture;


    float   lightFlickerRate = 0.07f;
    float   aniamtionRate = 0.013888888f;
    int     currentAnimationFrame = 0;

    float   timer = 0.0f;

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
    std::array<VkClearValue, 2> finalScenePassClearValues{};

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame = GetRenderTime();

    Ref<Image>       shadowMapImage;
    VkRenderPass     shadowMapRenderPass;
    Ref<Framebuffer> shadowMapFramebuffer;

    VkRenderPassBeginInfo depthPassBeginInfo;
    VkRenderPassBeginInfo finalScenePassBeginInfo;

    VkCommandBuffer cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool   cmdPool;

    // Helpers
    void WriteDescriptorSetWithUBOs(Model* model)
    {
        for (int i = 0; i < model->GetMeshes().size(); i++)
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

            bufferInfo.buffer = globalLightParametersUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GlobalLightPropertiesUBO);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = model->GetMeshes()[i]->GetDescriptorSet();
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }
    void WriteDescriptorSetSkybox(Model* model)
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
    void WriteDescriptorSetEmissiveObject(Model* model)
    {
        for (int i = 0; i < model->GetMeshes().size(); i++)
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
        specs.EnablePushConstant = false;
        
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
        
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(0);
        
        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.clear();
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;
        
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
        particleSpecs.EnablePushConstant = true;
        particleSpecs.PushConstantOffset = 0;
        particleSpecs.PushConstantShaderStage = VK_SHADER_STAGE_FRAGMENT_BIT;
        particleSpecs.PushConstantSize = sizeof(glm::vec4);


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
    void SetupEmissiveObjectPipeline()
    {
        // Emissive object pipeline.
        Pipeline::Specs specs{};
        specs.DescriptorLayout = emissiveLayout;
        specs.pRenderPass = &HDRRenderPass;
        specs.VertexShaderPath = "shaders/emissiveShaderVERT.spv";
        specs.FragmentShaderPath = "shaders/emissiveShaderFRAG.spv";
        specs.EnablePushConstant = true;
        specs.PushConstantOffset = 0;
        specs.PushConstantShaderStage = VK_SHADER_STAGE_VERTEX_BIT;
        specs.PushConstantSize = sizeof(glm::mat4) + sizeof(glm::vec4);
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

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.push_back(bindingDescription);

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;


        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        EmissiveObjectPipeline = std::make_shared<Pipeline>(specs);
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
        dependency.srcSubpass = 0;
        dependency.dstSubpass = VK_SUBPASS_EXTERNAL;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

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
        particleTexture = std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib/textures/spark.png") }, VK_FORMAT_R8G8B8A8_SRGB);
        fireTexture = std::make_shared<Image>(std::vector{ (std::string(SOLUTION_DIR) + "OVKLib/textures/fire_sprite_sheet.png")}, VK_FORMAT_R8G8B8A8_SRGB);

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
        fireSparks->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));

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
        fireBase->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));
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
        fireSparks2->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));

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
        fireBase2->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));
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
        fireSparks3->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));

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
        fireBase3->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));
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
        fireSparks4->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));

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
        fireBase4->SetUBO(globalParametersUBOBuffer, sizeof(GlobalParametersUBO));
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

        std::vector<DescriptorBindingSpecs> pbrLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(GlobalParametersUBO),         1, ShaderStage::VERTEX,    0},
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(GlobalLightPropertiesUBO),    1, ShaderStage::FRAGMENT,  1},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, ShaderStage::FRAGMENT , 2},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_NORMAL,             UINT64_MAX,                          1, ShaderStage::FRAGMENT , 3},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC,  UINT64_MAX,                          1, ShaderStage::FRAGMENT , 4},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_SHADOWMAP,          UINT64_MAX,                          1, ShaderStage::FRAGMENT , 5},
        };

        std::vector<DescriptorBindingSpecs> SkyboxLayout
        {
            DescriptorBindingSpecs {Type::UNIFORM_BUFFER,                      sizeof(glm::mat4),                   1, ShaderStage::VERTEX  ,  0},
            DescriptorBindingSpecs {Type::TEXTURE_SAMPLER_CUBEMAP,             UINT64_MAX,                          1, ShaderStage::FRAGMENT,  1}
        };

        std::vector<DescriptorBindingSpecs> ParticleSystemLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(GlobalParametersUBO),         1, ShaderStage::VERTEX   , 0}, // Index 0
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, ShaderStage::FRAGMENT , 1}, // Index 3
        };

        std::vector<DescriptorBindingSpecs> OneSamplerLayout
        {
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE,            UINT64_MAX,                          1, ShaderStage::FRAGMENT , 0},
        };

        std::vector<DescriptorBindingSpecs> EmissiveLayout
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,                     sizeof(GlobalParametersUBO),         1, ShaderStage::VERTEX,    0},
        };


        std::vector<VkDescriptorType> types;
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        PBRLayout = std::make_shared<DescriptorLayout>(pbrLayout);
        pool = std::make_shared<DescriptorPool>(200, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        skyboxLayout = std::make_shared<DescriptorLayout>(SkyboxLayout);
        pool2 = std::make_shared<DescriptorPool>(100, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        particleSystemLayout = std::make_shared<DescriptorLayout>(ParticleSystemLayout);
        particleSystemPool = std::make_shared<DescriptorPool>(100, types);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        oneSamplerLayout = std::make_shared<DescriptorLayout>(OneSamplerLayout);

        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        emissiveLayout = std::make_shared<DescriptorLayout>(EmissiveLayout);


        // Following are the global Uniform Buffes shared by all shaders.
        Utils::CreateVKBuffer(sizeof(GlobalParametersUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, globalParametersUBOBuffer, globalParametersUBOBufferMemory);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBufferMemory, 0, sizeof(GlobalParametersUBO), 0, &globalParametersModelUBOBuffer);
        
        Utils::CreateVKBuffer(sizeof(GlobalLightPropertiesUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, globalLightParametersUBOBuffer, globalLightParametersUBOBufferMemory);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), globalLightParametersUBOBufferMemory, 0, sizeof(GlobalLightPropertiesUBO), 0, &mappedGlobalLightParametersUBOBuffer);

        // Set the positions of the point lights in the scene we have 4 torches.
        globalLightParametersUBO.lightPositions[0] = glm::vec4(2.46f, 1.54f, 0.76f, 1.0f);
        globalLightParametersUBO.lightPositions[1] = glm::vec4(0.62f, 1.54f, 0.76f, 1.0f);
        globalLightParametersUBO.lightPositions[2] = glm::vec4(0.62f, 1.54f, -1.16f, 1.0f);
        globalLightParametersUBO.lightPositions[3] = glm::vec4(2.46f, 1.54f, -1.16f, 1.0f);
        globalLightParametersUBO.directionalLightIntensity.x = 10.0;

        SetupParticleSystems();

        // Create an image for the shadowmap. We will render to this image when we are doing a shadow pass.
        shadowMapImage = std::make_shared<Image>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, (VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT), ImageType::DEPTH);

        // Allocate final pass descriptor Set.
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &oneSamplerLayout->GetDescriptorLayout();

        VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &finalPassDescriptorSet);
        ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

        CreateHDRRenderPass();
        CreateHDRFramebuffer();

        CreateShadowRenderPass();

        SetupFinalPassPipeline();
        SetupPBRPipeline();
        SetupShadowPassPipeline();
        SetupSkyboxPipeline();
        SetupEmissiveObjectPipeline();
        SetupParticleSystemPipeline();


        std::vector<VkImageView> attachments =
        {
            shadowMapImage->GetImageView()
        };
        // Final piece of the puzzle is the framebuffer. We need a framebuffer to link the image we are rendering to with the render pass.
        shadowMapFramebuffer = std::make_shared<Framebuffer>(shadowMapRenderPass, attachments, SHADOW_DIM, SHADOW_DIM);
        
        // Loading the model Sponza
        model = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\Sponza\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, PBRLayout, shadowMapImage);
        model->Scale(0.005f, 0.005f, 0.005f);
        WriteDescriptorSetWithUBOs(model);

        // Loading the model Malenia's Helmet.
        model2 = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\MaleniaHelmet\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, PBRLayout, shadowMapImage);
        model2->Scale(0.7f, 0.7f, 0.7f);
        model2->Translate(2.0f, 2.0f, -0.2f);
        model2->Rotate(90, 0, 1, 0);
        WriteDescriptorSetWithUBOs(model2);

        torch = new Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\torch\\scene.gltf", LOAD_VERTEX_POSITIONS | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, PBRLayout, shadowMapImage);
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


        model3 = new OVK::Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\sword\\scene.gltf", LOAD_VERTEX_POSITIONS, pool, emissiveLayout);
        model3->Scale(0.7f, 0.7f, 0.7f);
        model3->Rotate(90, 0, 1, 0);
        model3->Rotate(54, 1, 0, 0);
        model3->Translate(0, 2, -5);
        WriteDescriptorSetEmissiveObject(model3);


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
        skybox = new Model(cubeVertices, vertexCount, cubemap, pool2, skyboxLayout);
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

        CommandBuffer::CreateCommandPool(s_GraphicsQueueFamily, cmdPool); 
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CommandBuffer::CreateCommandBuffer(cmdBuffers[i], cmdPool);
        }

        bloomAgent = std::make_shared<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloom(HDRColorImage);

        finalPassSampler = Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::WriteDescriptorSetWithSampler(finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


	}
    void OnUpdate()
    {
        // Begin command buffer recording.
        CommandBuffer::BeginRecording(cmdBuffers[CurrentFrameIndex()]);

        // Timer.
        float deltaTime;
        deltaTime = DeltaTime();
        timer += 7.0f * deltaTime;

        // Animating the light
        lightViewMatrix = glm::lookAt(glm::vec3(directionalLightPosition.x, directionalLightPosition.y, directionalLightPosition.z), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        depthMVP = lightProjectionMatrix * lightViewMatrix * lightModelMatrix;


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
            globalLightParametersUBO.lightIntensities[0] = glm::vec4(distr(gen));
            globalLightParametersUBO.lightIntensities[1] = glm::vec4(distr2(gen));
            globalLightParametersUBO.lightIntensities[2] = glm::vec4(distr3(gen));
            globalLightParametersUBO.lightIntensities[3] = glm::vec4(distr4(gen));
        }

        memcpy(mappedGlobalLightParametersUBOBuffer, &globalLightParametersUBO, sizeof(globalLightParametersUBO));
        glm::mat4 view = s_Camera->GetViewMatrix();
        glm::mat4 proj = s_Camera->GetProjectionMatrix();
        glm::vec4 cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);

        globalParametersUBO.viewMatrix = view;
        globalParametersUBO.projMatrix = proj;
        globalParametersUBO.cameraPosition = cameraPos;
        globalParametersUBO.dirLightPos = directionalLightPosition;
        globalParametersUBO.depthMVP = depthMVP;
        globalParametersUBO.viewportDimension = glm::vec4(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height, 0.0f, 0.0f);
        memcpy(globalParametersModelUBOBuffer, &globalParametersUBO, sizeof(globalParametersUBO));
        
        glm::mat4 mat = model->GetModelMatrix();
        model2->Rotate(5.0f * deltaTime, 0.0, 1, 0.0f);
        glm::mat4 mat2 = model2->GetModelMatrix();

        VkDeviceSize offset = 0;

        // Start shadow pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], depthPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPassPipeline);

        // Render the objects you want to cast shadows.
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
        model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout());
        
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
        model2->DrawIndexed(cmdBuffers[CurrentFrameIndex()], shadowPassPipeline->GetPipelineLayout());
        
        // End shadow pass.
        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);




        
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

        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], HDRRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Drawing the skybox.
        glm::mat4 skyBoxView = glm::mat4(glm::mat3(view));
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], skyboxPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &skyBoxView);
        skybox->Draw(cmdBuffers[CurrentFrameIndex()], skyboxPipeline->GetPipelineLayout());

        // Drawing the Sponza.
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat);
        model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());

        // Drawing the helmet.
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &mat2);
        model2->DrawIndexed(cmdBuffers[CurrentFrameIndex()], pipeline->GetPipelineLayout());


        // Drawing 4 torches.
        for (int i = 0; i < torch->GetMeshes().size(); i++)
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


        // Draw the emissive sword.
        glm::mat4 swordMat = model3->GetModelMatrix();
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &swordMat);
        model3->DrawIndexed(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout());

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
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystemPipeline);

        glm::vec4 sparkBrigtness;
        sparkBrigtness.x = 15.0f;
        glm::vec4 flameBrigthness;
        flameBrigthness.x = 8.0f;

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


        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);

        // Apply bloom to the previously rendered image HDRColorImage.
        bloomAgent->ApplyBloom(cmdBuffers[CurrentFrameIndex()]);

        finalScenePassClearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };

        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetActiveFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(finalScenePassClearValues.size());
        finalScenePassBeginInfo.pClearValues = finalScenePassClearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[GetActiveImageIndex()]->GetVKFramebuffer();

        // Start final scene render pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline);
        vkCmdBindDescriptorSets(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline->GetPipelineLayout(), 0, 1, &finalPassDescriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffers[CurrentFrameIndex()], 3, 1, 0, 0);

        // End the command buffer recording phase.
        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
        CommandBuffer::EndRecording(cmdBuffers[CurrentFrameIndex()]);


        SubmitCommandBuffer(cmdBuffers[CurrentFrameIndex()]);
    }
    void OnCleanup()
    {
        delete model;
        delete model2;
        delete model3;
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
            CommandBuffer::FreeCommandBuffer(cmdBuffers[i], cmdPool, s_Device->GetGraphicsQueue());
        }
        CommandBuffer::DestroyCommandPool(cmdPool);
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), shadowMapRenderPass, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBufferMemory, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), globalLightParametersUBOBufferMemory, nullptr);
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), globalParametersUBOBuffer, nullptr);
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), globalLightParametersUBOBuffer, nullptr);
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), finalPassSampler, nullptr);
        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), HDRRenderPass, nullptr);

    }
    void OnWindowResize()
    {
        pipeline->OnResize();
        shadowPassPipeline->OnResize();
        skyboxPipeline->OnResize();
        particleSystemPipeline->OnResize();
        finalPassPipeline->OnResize();
        EmissiveObjectPipeline->OnResize();

        CreateHDRFramebuffer();

        bloomAgent = std::make_shared<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloom(HDRColorImage);

        finalPassSampler = Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::WriteDescriptorSetWithSampler(finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
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