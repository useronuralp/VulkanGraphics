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
#include <random>

#define ENABLE_VALIDATION false
#define PARTICLE_COUNT 512
#define PARTICLE_SIZE 0.1f

#define FLAME_RADIUS 8.0f

#define PARTICLE_TYPE_FLAME 0
#define PARTICLE_TYPE_SMOKE 1
const float M_PI = 3.14159265359;


using namespace Anor;
class MyApplication : public Anor::VulkanApplication
{
    struct Particle
    {
        glm::vec4 pos;
        glm::vec4 color;
        float alpha;
        float size;
        float rotation;
        uint32_t type;
        // Attributes not used in shader
        glm::vec4 vel;
        float rotationSpeed;
    };


    const uint32_t SHADOW_DIM = 8000;
public:
    Model* model;
    Model* model2;
    Model* lightSphere;
    Mesh* lightSphereMesh;
    Mesh* skybox;
    Mesh* debugQuad;
    float timer = 0.0f;
    glm::vec3 pointLightPosition = glm::vec3(50.0f, 50.0f, 50.0f);
    glm::vec3 directionalLightPosition = glm::vec3(35.0f, 35.0f, 9.0f);
    float near_plane = 5.0f;
    float far_plane = 200.0f;
    int frameCount = 0;

    glm::mat4 lightView = glm::lookAt(directionalLightPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 lightProjectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, near_plane, far_plane);
    glm::mat4 lightModel = glm::mat4(1.0f);
    glm::mat4 depthMVP = lightProjectionMatrix * lightView * lightModel;

    VkClearValue clearValue;

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame;
    Ref<Texture> shadowMapTexture;

    Ref<RenderPass> shadowMapPass;
    Ref<Framebuffer> shadowMapFramebuffer;

    VkRenderPassBeginInfo depthPassBeginInfo;
    VkRenderPassBeginInfo finalScenePassBeginInfo;


    // EXPERIMENTAL PARTICLES
    glm::vec3 emitterPos = glm::vec3(-10.0f, FLAME_RADIUS + 2.0f, 0.0f);
    glm::vec3 minVel = glm::vec3(-3.0f, 0.5f, -3.0f);
    glm::vec3 maxVel = glm::vec3(3.0f, 7.0f, 3.0f);

    VkBuffer particleBuffer;
    VkDeviceMemory particleBufferMemory;
    size_t particleBufferSize;

    void* mappedParticleBuffer; //Used to update the buffer every frame.

    Ref<UniformBuffer> particelSytemUBO;
    Ref<Pipeline> particeSystemPipeline;
    Ref<DescriptorSet> particlSystemDescriptorSet;
    VkSampler particleSampler;
    VkRenderPassBeginInfo particleSystemRenderPassBeginInfo;
    Ref<Texture> particleTexture;

    Ref<UniformBuffer> projUBO;
    Ref<UniformBuffer> modelViewUBO;
    Ref<UniformBuffer> viewportDimUBO;


    std::vector<Particle> particles;


    std::default_random_engine rndEngine;
    float rnd(float range)
    {
        std::uniform_real_distribution<float> rndDist(0.0f, range);
        return rndDist(rndEngine);
    }


    void InitParticle(Particle* particle, glm::vec3 emitterPos)
    {
        particle->vel = glm::vec4(-10.0f, minVel.y + rnd(maxVel.y - minVel.y), 0.0f, 0.0f);
        particle->alpha = 1.0f;
        particle->size = 1.0f + rnd(0.5f);
        particle->color = glm::vec4(1.0f);
        particle->type = PARTICLE_TYPE_FLAME;
        particle->rotation = rnd(2.0f * M_PI);
        particle->rotationSpeed = rnd(2.0f) - rnd(2.0f);

        // Get random sphere point
        float theta = rnd(2.0f * M_PI);
        float phi = rnd(M_PI) - M_PI / 2.0f;
        float r = rnd(FLAME_RADIUS);

        particle->pos.x = r * cos(theta) * cos(phi);
        particle->pos.y = r * sin(phi);
        particle->pos.z = r * sin(theta) * cos(phi);

        particle->pos += glm::vec4(emitterPos, 0.0f);
    }

    void prepareParticles()
    {
        particles.resize(PARTICLE_COUNT);
        for (auto& particle : particles)
        {
            InitParticle(&particle, emitterPos);
            //particle.alpha = 1.0f - (abs(particle.pos.y) / (FLAME_RADIUS * 2.0f));
        }

        particleBufferSize = particles.size() * sizeof(Particle);

        // Create a buffer and copy over the particle data into it. We also map this buffer so that we can change the data contained in there every frame.
        Utils::CreateVKBuffer(particleBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, particleBuffer, particleBufferMemory);
        ASSERT(vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), particleBufferMemory, 0, particleBufferSize, 0, &mappedParticleBuffer) == VK_SUCCESS, "Failed to map the memory");
        memcpy(mappedParticleBuffer, particles.data(), particleBufferSize);
    }

    void updateParticles(float deltaTime)
    {
        float particleTimer = deltaTime * 0.01f;
        for (auto& particle : particles)
        {
            particle.pos.y -= particle.vel.y * particleTimer * 3.5f;
            particle.pos.x -= particle.vel.x * particleTimer * 3.5f;
            particle.alpha -= particleTimer * 2.5f;
            particle.size -= particleTimer * 0.5f;
            particle.rotation += particleTimer * particle.rotationSpeed;
        }
        size_t size = particles.size() * sizeof(Particle);
        memcpy(mappedParticleBuffer, particles.data(), size);
    }

    void SetupParticles()
    {
        prepareParticles();

        std::vector<DescriptorLayout> dscLayout
        {
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 0
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       ShaderStage::VERTEX   }, // Index 1
            DescriptorLayout { Type::UNIFORM_BUFFER,  Size::VEC2,                       ShaderStage::VERTEX   }, // Index 2
            DescriptorLayout { Type::TEXTURE_SAMPLER,  Size::DIFFUSE_SAMPLER,           ShaderStage::FRAGMENT }, // Index 3

        };

        particlSystemDescriptorSet = std::make_shared<DescriptorSet>(dscLayout);

        Pipeline::Specs particleSpecs{};
        particleSpecs.DescriptorSetLayout = particlSystemDescriptorSet->GetVKDescriptorSetLayout();
        particleSpecs.RenderPass = VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
        particleSpecs.CullMode = VK_CULL_MODE_NONE;
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
        attributeDescriptions.resize(6);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Particle, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Particle, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Particle, alpha);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Particle, size);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Particle, rotation);

        attributeDescriptions[5].binding = 0;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32_SINT;
        attributeDescriptions[5].offset = offsetof(Particle, type);

        particleSpecs.VertexBindingDesc = bindingDescription;
        particleSpecs.pVertexAttributeDescriptons = attributeDescriptions;

        particeSystemPipeline = std::make_shared<Pipeline>(particleSpecs);
        rndEngine.seed((unsigned)time(nullptr));


        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;

        // Repeats the texture when going out of the sampling range. You might wanna expose this variable during ImageBufferCreation.
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        ASSERT(vkCreateSampler(s_Device->GetVKDevice(), &samplerInfo, nullptr, &particleSampler) == VK_SUCCESS, "Failed to create particle sampler!");

        particleTexture = std::make_shared<Texture>((std::string(SOLUTION_DIR) + "Anorlib/textures/particle_texture.png").c_str(), VK_FORMAT_R8G8B8A8_SRGB);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = particleTexture->GetImage()->GetImageView();
        imageInfo.sampler = particleSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = particlSystemDescriptorSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = 3;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

        projUBO = std::make_shared<UniformBuffer>(particlSystemDescriptorSet, sizeof(glm::mat4), 0 );
        modelViewUBO = std::make_shared<UniformBuffer>(particlSystemDescriptorSet, sizeof(glm::mat4), 1);
        viewportDimUBO = std::make_shared<UniformBuffer>(particlSystemDescriptorSet, sizeof(glm::vec2), 2);
    }

    void updateParticleUBOs()
    {
        glm::mat4 proj = GetCameraProjectionMatrix();
        glm::mat4 modelView = GetCameraViewMatrix();
        glm::vec2 viewportDim = glm::vec2(s_Surface->GetVKExtent().width, s_Surface->GetVKExtent().height);

        projUBO->UpdateUniformBuffer(&proj, sizeof(proj));
        modelViewUBO->UpdateUniformBuffer(&modelView, sizeof(modelView));
        viewportDimUBO->UpdateUniformBuffer(&viewportDim, sizeof(viewportDim));
    }


    void SetupScenePassConfiguration(Model* model)
    {
        // Specify the layout of the descriptor set that you'll use in your shaders.
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

        // Configure the pipeline spcifications. All of these fields except the "DescriptorSetLayout" is mandatory.
        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = VK_NULL_HANDLE; 
        specs.RenderPass = VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
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
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(5);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[2].offset = offsetof(Vertex, normal);

        attributeDescriptions[3].binding = 0;
        attributeDescriptions[3].location = 3;
        attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[3].offset = offsetof(Vertex, tangent);

        attributeDescriptions[4].binding = 0;
        attributeDescriptions[4].location = 4;
        attributeDescriptions[4].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[4].offset = offsetof(Vertex, bitangent);

        specs.VertexBindingDesc = bindingDescription;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        // Finally, add the newly created configuration to the model to be used when drawing.
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
        specs.EnableDepthBias = VK_TRUE;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/shadowPassVERT.spv";
        specs.FragmentShaderPath = "shaders/shadowPassFRAG.spv";
        specs.ViewportHeight = SHADOW_DIM;
        specs.ViewportWidth = SHADOW_DIM;

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
        bindingDescription.stride = sizeof(Vertex); // Since we've loaded this model using the Model class all the extra information has been loaded regardless of us defining the VertexInputState. We still need the stride to be the Vertex.
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);
        
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        specs.VertexBindingDesc = bindingDescription;
        specs.pVertexAttributeDescriptons = attributeDescriptions;


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
        specs.RenderPass = VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
        specs.CullMode = VK_CULL_MODE_NONE;
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
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(1);
        // For position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        specs.VertexBindingDesc = bindingDescription;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        model->AddConfiguration("NormalRenderPass", specs, dscLayout);
        model->SetActiveConfiguration("NormalRenderPass");
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
        specs.RenderPass = VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
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

        specs.VertexBindingDesc = bindingDescription;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        mesh->AddConfiguration("NormalRenderPass", specs, dscLayout);
    }
    void SetupDebugQuad(Mesh* mesh)
    {
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
        specs.RenderPass = VulkanApplication::s_Swapchain->GetSwapchainRenderPass();
        specs.CullMode = VK_CULL_MODE_NONE;
        specs.DepthBiasClamp = 0.0f;
        specs.DepthBiasConstantFactor = 0.0f;
        specs.DepthBiasSlopeFactor = 0.0f;
        specs.DepthCompareOp = VK_COMPARE_OP_LESS;
        specs.EnableDepthBias = false;
        specs.EnableDepthTesting = VK_TRUE;
        specs.EnableDepthWriting = VK_TRUE;
        specs.FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        specs.PolygonMode = VK_POLYGON_MODE_FILL;
        specs.VertexShaderPath = "shaders/debugQuadVERT.spv";
        specs.FragmentShaderPath = "shaders/debugQuadFRAG.spv";

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
        bindingDescription.stride = sizeof(glm::vec3) + sizeof(glm::vec2);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(2);
        // For position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = sizeof(glm::vec3);

        specs.VertexBindingDesc = bindingDescription;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        mesh->AddConfiguration("NormalRenderPass", specs, dscLayout);
    }
    void CreateShadowRenderPass()
    {
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
    }

private:
    void OnStart()
    {
        SetupParticles();

        // Create a texture for the shadowmap. We will render to this texture when we are doing a shadow pass.
        shadowMapTexture = std::make_shared<Texture>(SHADOW_DIM, SHADOW_DIM, VK_FORMAT_D32_SFLOAT, ImageType::DEPTH);

        // Creating our shadow pass with this helper function.
        CreateShadowRenderPass();

        // Final piece of the puzzle is the framebuffer. We need a framebuffer to link the image we are rendering to with the render pass.
        shadowMapFramebuffer = std::make_shared<Framebuffer>(shadowMapPass, shadowMapTexture);

        model = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\Sponza\\scene.gltf");
        model->SetShadowMap(shadowMapTexture);
        SetupScenePassConfiguration(model);
        SetupShadowPassConfiguration(model);
        model->SetActiveConfiguration("NormalRenderPass");
        model->Scale(0.005f, 0.005f, 0.005f);

        model2 = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\MaleniaHelmet\\scene.gltf");
        model2->SetShadowMap(shadowMapTexture);
        SetupScenePassConfiguration(model2);
        SetupShadowPassConfiguration(model2);
        model2->SetActiveConfiguration("NormalRenderPass");
        model2->Scale(0.7f, 0.7f, 0.7f);
        
	    lightSphere = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\Sphere\\scene.gltf");
        SetupLightSphere(lightSphere);
        lightSphere->SetActiveConfiguration("NormalRenderPass");
        lightSphere->Scale(0.002f, 0.002f, 0.002f);


        // TO DO: Remove unnecessart vertex data from here.
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

        const float quadVertices[5 * 4] =
        {
             -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
              1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
              1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
             -1.0f,  1.0f, 0.0f,  0.0f, 0.0f,
        };

        std::vector<uint32_t> quadIndices = { 0, 1, 2, 2, 3, 0 };

        debugQuad = new Mesh(quadVertices, (size_t)(sizeof(float) * 5 * 4), 4, quadIndices);
        debugQuad->SetShadowMap(shadowMapTexture);
        SetupDebugQuad(debugQuad);
        debugQuad->SetActiveConfiguration("NormalRenderPass");
        debugQuad->Translate(5.0f, 7.4f, 0.0f);

        std::string front   = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\front.jpg");
        std::string back    = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\back.jpg");
        std::string top     = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\top.jpg");
        std::string bottom  = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\bottom.jpg");
        std::string right   = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\right.jpg");
        std::string left    = (std::string(SOLUTION_DIR) + "Anorlib\\textures\\skybox\\MountainPath\\left.jpg");


        std::array<std::string, 6> skyboxTex { right, left, top, bottom, front, back};
        Ref<Anor::CubemapTexture> cubemap = std::make_shared<CubemapTexture>(skyboxTex, VK_FORMAT_R8G8B8A8_SRGB);

        skybox = new Mesh(cubeVertices, (size_t)(sizeof(float) * 3 * 6 * 6), 6 * 6, cubemap);

        SetupSkybox(skybox);
        skybox->SetActiveConfiguration("NormalRenderPass");

        m_LastFrameRenderTime = GetRenderTime();

        // Configure the render pass begin info for the depth pass here.
        clearValue = { 1.0f, 0.0 };

        depthPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        depthPassBeginInfo.renderPass = shadowMapPass->GetRenderPass();
        depthPassBeginInfo.framebuffer = shadowMapFramebuffer->GetVKFramebuffer();
        depthPassBeginInfo.renderArea.offset = { 0, 0 };
        depthPassBeginInfo.renderArea.extent.width = shadowMapFramebuffer->GetWidth();
        depthPassBeginInfo.renderArea.extent.height = shadowMapFramebuffer->GetHeight();
        depthPassBeginInfo.clearValueCount = 1;
        depthPassBeginInfo.pClearValues = &clearValue;
	}
    void OnUpdate()
    {
        float currentTime, deltaTime;
        deltaTime = DeltaTime();
        // FPS Counter
        currentTime = GetRenderTime();
        //printf("%.2f\n", deltaTime / 1000);
        frameCount++;
        if (currentTime - m_LastFrameRenderTime >= 1.0)
        {
            std::cout << frameCount << std::endl;

            frameCount = 0;
            m_LastFrameRenderTime = currentTime;
        }

        timer += 7.0f * deltaTime;

        // Animating the light
        directionalLightPosition.z = std::sin(glm::radians(timer )) * 9.0f;
        lightView = glm::lookAt(directionalLightPosition, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        depthMVP = lightProjectionMatrix * lightView * lightModel;
        pointLightPosition = glm::vec3(lightSphere->GetModelMatrix()[3][0], lightSphere->GetModelMatrix()[3][1], lightSphere->GetModelMatrix()[3][2]);


        glm::mat4 view = GetCameraViewMatrix();         
        glm::mat4 proj = GetCameraProjectionMatrix();   
        glm::vec3 cameraPos = GetCameraPosition();      

        glm::mat mat = model->GetModelMatrix();
        model2->Translate(0.0, 0.2f * deltaTime, 0.0f);
        glm::mat mat2 = model2->GetModelMatrix();
        
        // Beginning of the custom render pass.
        BeginCustomRenderPass(depthPassBeginInfo);
        
        model2->SetActiveConfiguration("ShadowMapPass");
        model2->UpdateUniformBuffer(0, &mat2, sizeof(mat2));
        model2->UpdateUniformBuffer(1, &depthMVP, sizeof(depthMVP));
        model2->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.
        
        model->SetActiveConfiguration("ShadowMapPass");
        model->UpdateUniformBuffer(0, &mat, sizeof(mat));
        model->UpdateUniformBuffer(1, &depthMVP, sizeof(depthMVP));
        model->DrawIndexed(GetActiveCommandBuffer()); // Call low level draw calls in here.
        
        EndDepthPass();
        // End of the custom render pass.

        // Beginning of the render pass thet will draw images to the swapchain.
        BeginRenderPass();
        
        updateParticles(deltaTime);
        updateParticleUBOs();
        model->SetActiveConfiguration("NormalRenderPass");
        model2->SetActiveConfiguration("NormalRenderPass");
        
        glm::mat4 skyBoxView = glm::mat4(glm::mat3(view));
        skybox->UpdateUniformBuffer(0, &skyBoxView, sizeof(skyBoxView));
        skybox->UpdateUniformBuffer(1, &proj, sizeof(proj));
        skybox->Draw(GetActiveCommandBuffer());
        
        
        model2->UpdateUniformBuffer(0, &mat2, sizeof(mat2));
        model2->UpdateUniformBuffer(1, &view, sizeof(view));
        model2->UpdateUniformBuffer(2, &proj, sizeof(proj));
        model2->UpdateUniformBuffer(3, &depthMVP, sizeof(depthMVP));
        model2->UpdateUniformBuffer(4, &directionalLightPosition, sizeof(directionalLightPosition));
        model2->UpdateUniformBuffer(5, &cameraPos, sizeof(cameraPos));
        model2->UpdateUniformBuffer(6, &pointLightPosition, sizeof(pointLightPosition));
        
        model2->DrawIndexed(GetActiveCommandBuffer()); 
        
        
        model->UpdateUniformBuffer(0, &mat, sizeof(mat)); 
        model->UpdateUniformBuffer(1, &view, sizeof(view));
        model->UpdateUniformBuffer(2, &proj, sizeof(proj));
        model->UpdateUniformBuffer(3, &depthMVP, sizeof(depthMVP));
        model->UpdateUniformBuffer(4, &directionalLightPosition, sizeof(directionalLightPosition));
        model->UpdateUniformBuffer(5, &cameraPos, sizeof(cameraPos));
        model->UpdateUniformBuffer(6, &pointLightPosition, sizeof(pointLightPosition));
        
        model->DrawIndexed(GetActiveCommandBuffer());

        VkDeviceSize offsets[1] = { 0 };

        vkCmdBindDescriptorSets(GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, particeSystemPipeline->GetPipelineLayout(), 0, 1, &particlSystemDescriptorSet->GetVKDescriptorSet(), 0, nullptr);
        vkCmdBindPipeline(GetActiveCommandBuffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, particeSystemPipeline->GetVKPipeline());
        vkCmdBindVertexBuffers(GetActiveCommandBuffer(), 0, 1, &particleBuffer, offsets);
        vkCmdDraw(GetActiveCommandBuffer(), PARTICLE_COUNT, 1, 0, 0);
        
        //debugQuad->UpdateUniformBuffer(1, &proj, sizeof(proj));
        //debugQuad->UpdateUniformBuffer(2, &view, sizeof(view));
        //glm::mat4 temp = lightProjectionMatrix * lightView * glm::mat4(1.0);
        //debugQuad->UpdateUniformBuffer(3, &temp, sizeof(temp));
        //debugQuad->DrawIndexed(GetActiveCommandBuffer());


        lightSphere->UpdateUniformBuffer(1, &view, sizeof(view));
        lightSphere->UpdateUniformBuffer(2, &proj, sizeof(proj));
        lightSphere->Translate(0.0f, 10.0f * deltaTime, 10.0f * deltaTime);
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
        particeSystemPipeline->OnResize();
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