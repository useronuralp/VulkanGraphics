// Native code
#include "VulkanApplication.h"
#include "Model.h"
#include "Framebuffer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "LogicalDevice.h"
#include "RenderPass.h"
#include "Utils.h"
#include "Window.h"
#include "CommandBuffer.h"
#include "Camera.h"
// External libs
#include <random>
// -----------------------------------------------------
// -----------------------------------------------------
/*
*   Test #1: define "SEPARATE_DRAW_CALL_TEST" 
*   Test #2: define "INSTANCED_DRAWING_TEST"
*/
#define INSTANCED_DRAWING_TEST
#define OBJECT_COUNT 10000
// -----------------------------------------------------
// -----------------------------------------------------

#define MAX_FRAMES_IN_FLIGHT 2 
int CURRENT_FRAME = 0;
using namespace Anor;
class MyApplication : public Anor::VulkanApplication
{
public:
    // Vulkan Objects
    VkClearValue            depthPassClearValue;
    VkRenderPassBeginInfo   finalScenePassBeginInfo;
    VkCommandBuffer         cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool           cmdPools[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore             imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore             renderingCompleteSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence                 inRenderingFences[MAX_FRAMES_IN_FLIGHT];
    VkCommandBuffer         commandBuffer;
    VkCommandPool           commandPool;
    VkDeviceSize            offsets;
    VkSampler               diffuseSampler;
    VkSampler               normalSampler;
    VkSampler               RMSampler;

    // Buffers
    VkBuffer                modelMatUBOBuffer;
    VkDeviceMemory          modelMatUBOBufferMemory;
    void*                   mappedModelUBO;

    VkBuffer                projMatUBOBuffer;
    VkDeviceMemory          projMatUBOBufferMemory;
    void*                   mappedProjUBO;

    VkBuffer                viewMatUBOBuffer;
    VkDeviceMemory          viewMatUBOBufferMemory;
    void*                   mappedViewlUBO;

    VkBuffer                dirLightPosUBOBuffer;
    VkDeviceMemory          dirLightPosUBOBufferMemory;
    void*                   mappedDirLightUBO;

    VkBuffer                testFlagUBOBuffer;
    VkDeviceMemory          testFlagUBOMemory;
    void*                   mappedTestFlagUBO;

    VkBuffer                camPosUBOBuffer;
    VkDeviceMemory          camPosUBOBufferMemory;
    void*                   mappedCamPosUBO;

    VkBuffer                instancedDataBuffer;
    VkDeviceMemory          instancedDataMemory;
    void*                   mappedInstancedData;

    // Rest of the variables
    std::vector<DescriptorSetLayout> dscLayout;
    std::default_random_engine rndEngine;

    std::vector<Mesh> meshes;
    Model*  modelLoad;

    std::vector<glm::vec3>   positions;
    std::vector<glm::vec3>   rotations;
    glm::vec4   directionalLightPosition = glm::vec4(-300.0f, 600.0f, -600.0f, 1.0f);

    float near_plane = 5.0f;
    float far_plane = 200.0f;
    int   frameCount = 0;

    std::array<VkClearValue, 2> scenePassClearValues{};

    float m_LastFrameRenderTime;
    float m_DeltaTimeLastFrame = GetRenderTime();

    Ref<Pipeline>           pipeline;
    Ref<Texture>            diffuse;
    Ref<Texture>            normal;
    Ref<Texture>            RM;
    Ref<Texture>            shadowMap = nullptr;
    std::vector<glm::mat4>  modelMatrices;
    std::vector<Ref<DescriptorSet>> dscSets;

    // Helper random number generator.
    float rnd(float min, float max)
    {
        std::uniform_real_distribution<float> rndDist(min, max);
        return rndDist(rndEngine);
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
        modelMatrices.resize(OBJECT_COUNT);
        dscSets.resize(OBJECT_COUNT);
        rotations.resize(OBJECT_COUNT);
        positions.resize(OBJECT_COUNT);
        meshes.resize(OBJECT_COUNT);

        offsets = 0;
        // Specify the layout of the descriptor set that you'll use in your shaders.
        dscLayout =
        {
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       1, ShaderStage::VERTEX   , 0},
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       1, ShaderStage::VERTEX   , 1},
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::MAT4,                       1, ShaderStage::VERTEX   , 2},
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::VEC4,                       1, ShaderStage::VERTEX   , 3},
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::FLOAT,                      1, ShaderStage::VERTEX   , 4},
            DescriptorSetLayout { Type::UNIFORM_BUFFER,  Size::VEC4,                       1, ShaderStage::FRAGMENT , 5},
            DescriptorSetLayout { Type::TEXTURE_SAMPLER, Size::DIFFUSE_SAMPLER,            1, ShaderStage::FRAGMENT , 6},
            DescriptorSetLayout { Type::TEXTURE_SAMPLER, Size::NORMAL_SAMPLER,             1, ShaderStage::FRAGMENT , 7},
            DescriptorSetLayout { Type::TEXTURE_SAMPLER, Size::ROUGHNESS_METALLIC_SAMPLER, 1, ShaderStage::FRAGMENT , 8},
        };

        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            dscSets[i] = std::make_shared<DescriptorSet>(dscLayout);
        }
        modelLoad   = new Anor::Model(std::string(SOLUTION_DIR) + "Anorlib\\models\\sausage\\scene.gltf", LOAD_VERTICES | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV);
        diffuse     = modelLoad->GetMeshes()[0]->GetAlbedo();
        normal      = modelLoad->GetMeshes()[0]->GetNormals();
        RM          = modelLoad->GetMeshes()[0]->GetRoughnessMetallic();
        shadowMap   = nullptr;

        // Pipeline--------------------------------------------
        Pipeline::Specs specs{};
        specs.DescriptorSetLayout = dscSets[0]->GetVKDescriptorSetLayout();
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
        specs.VertexShaderPath = "shaders/shaderTestVERT.spv";
        specs.FragmentShaderPath = "shaders/shaderTestFRAG.spv";
        specs.PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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

        VkVertexInputBindingDescription bindingDescription2{};
        bindingDescription2.binding = 1;
        bindingDescription2.stride = sizeof(glm::mat4);
        bindingDescription2.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

        // PER VERTEX DATA
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(9);
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

        // INSTANCED DATA
        attributeDescriptions[5].binding = 1;
        attributeDescriptions[5].location = 5;
        attributeDescriptions[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[5].offset = 0;

        attributeDescriptions[6].binding = 1;
        attributeDescriptions[6].location = 6;
        attributeDescriptions[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[6].offset = sizeof(glm::vec4);

        attributeDescriptions[7].binding = 1;
        attributeDescriptions[7].location = 7;
        attributeDescriptions[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[7].offset = sizeof(glm::vec4) + sizeof(glm::vec4);

        attributeDescriptions[8].binding = 1;
        attributeDescriptions[8].location = 8;
        attributeDescriptions[8].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[8].offset = sizeof(glm::vec4) + sizeof(glm::vec4) + sizeof(glm::vec4);

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.resize(2);
        bindingDescs[0] = bindingDescription;
        bindingDescs[1] = bindingDescription2;
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        pipeline = std::make_shared<Pipeline>(specs);

        // UBOs
        // A single large model matrix buffer for every single object.
        VkDeviceSize bufferSize = sizeof(glm::mat4) * OBJECT_COUNT;
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, modelMatUBOBuffer, modelMatUBOBufferMemory);
        // View matrix buffer.
        bufferSize = sizeof(glm::mat4);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, viewMatUBOBuffer, viewMatUBOBufferMemory);
        // Projection matrix buffer.
        bufferSize = sizeof(glm::mat4);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, projMatUBOBuffer, projMatUBOBufferMemory);
        // Directional light position buffer.
        bufferSize = sizeof(glm::vec4);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, dirLightPosUBOBuffer, dirLightPosUBOBufferMemory);
        // Directional light position buffer.
        bufferSize = sizeof(float);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, testFlagUBOBuffer, testFlagUBOMemory);
        // Camera position buffer.
        bufferSize = sizeof(glm::vec4);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, camPosUBOBuffer, camPosUBOBufferMemory);
        
        // Instanced data buffer that changes every frame.
        bufferSize = sizeof(glm::mat4) * OBJECT_COUNT;
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instancedDataBuffer, instancedDataMemory);

        // Sampler creations
        Utils::CreateSampler(diffuse, diffuseSampler);
        Utils::CreateSampler(normal, normalSampler);
        Utils::CreateSampler(RM, RMSampler);
       

        // Write descriptor sets of every single object.
        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            // Create a sossig.
            meshes[i] = Mesh(modelLoad->GetMeshes()[0]->GetVBO(), modelLoad->GetMeshes()[0]->GetIBO(), diffuse, normal, RM, shadowMap);
            meshes[i].GetModelMatrix() = glm::scale(meshes[i].GetModelMatrix(), glm::vec3(0.03f, 0.03f, 0.03f));

            // Write the corresponding area in the big model matrix buffer.
            VkDescriptorBufferInfo bufferInfo{};
            VkDescriptorImageInfo imageInfo{};
            bufferInfo.buffer = modelMatUBOBuffer;
            bufferInfo.offset = i * sizeof(glm::mat4);
            bufferInfo.range = sizeof(glm::mat4);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            bufferInfo.buffer = viewMatUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::mat4);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 1;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            bufferInfo.buffer = projMatUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::mat4);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 2;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);


            bufferInfo.buffer = dirLightPosUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::vec4);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 3;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            bufferInfo.buffer = testFlagUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(float);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 4;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);


            bufferInfo.buffer = camPosUBOBuffer;
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::vec4);

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 5;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);


            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = diffuse->GetImage()->GetImageView();
            imageInfo.sampler = diffuseSampler;

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 6;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = normal->GetImage()->GetImageView();
            imageInfo.sampler = normalSampler;

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 7;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = RM->GetImage()->GetImageView();
            imageInfo.sampler = RMSampler;

            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = dscSets[i]->GetVKDescriptorSet();
            descriptorWrite.dstBinding = 8;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;
            vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);

            rotations[i] = glm::vec3(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f));
            positions[i] = glm::vec3(300 * rnd(-1.0f, 1.0f), 300 * rnd(-1.0f, 1.0f), 300 * rnd(-1.0f, 1.0f));

            meshes[i].GetModelMatrix() = glm::translate(meshes[i].GetModelMatrix(), positions[i]);
            meshes[i].GetModelMatrix() = glm::rotate(meshes[i].GetModelMatrix(), glm::radians(70.0f), glm::vec3(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)));
        }
       
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

        // Map the buffers e want to update each frame. (All in this case)
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), modelMatUBOBufferMemory, 0, sizeof(glm::mat4) * OBJECT_COUNT, 0, &mappedModelUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), viewMatUBOBufferMemory, 0, sizeof(glm::mat4), 0, &mappedViewlUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), projMatUBOBufferMemory, 0, sizeof(glm::mat4), 0, &mappedProjUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), dirLightPosUBOBufferMemory, 0, sizeof(glm::vec4), 0, &mappedDirLightUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), testFlagUBOMemory, 0, sizeof(float), 0, &mappedTestFlagUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), camPosUBOBufferMemory, 0, sizeof(glm::vec4), 0, &mappedCamPosUBO);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), instancedDataMemory, 0, sizeof(glm::mat4) * OBJECT_COUNT, 0, &mappedInstancedData);

        float flag;
        memcpy(mappedDirLightUBO, &directionalLightPosition, sizeof(directionalLightPosition));
#ifdef INSTANCED_DRAWING_TEST
        flag = 1.0f;
        memcpy(mappedTestFlagUBO, &flag, sizeof(flag));
#else
        flag = 0.0f;
        memcpy(mappedTestFlagUBO, &flag, sizeof(flag));
#endif 

    }
    void OnUpdate()
    {
        vkWaitForFences(s_Device->GetVKDevice(), 1, &inRenderingFences[CURRENT_FRAME], VK_TRUE, UINT64_MAX);
        float currentTime, deltaTime;
        deltaTime = DeltaTime();
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


        glm::mat4 view = s_Camera->GetViewMatrix();
        glm::mat4 proj = s_Camera->GetProjectionMatrix();
        glm::vec4 cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);

        memcpy(mappedViewlUBO, &view, sizeof(view));
        memcpy(mappedProjUBO, &proj, sizeof(proj));
        memcpy(mappedCamPosUBO, &cameraPos, sizeof(cameraPos));
        
        // Setup the actual scene render pass here.
        scenePassClearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };
        scenePassClearValues[1].depthStencil = { 1.0f, 0 };

        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass()->GetRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[imageIndex]->GetVKFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(scenePassClearValues.size());
        finalScenePassBeginInfo.pClearValues = scenePassClearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[imageIndex]->GetVKFramebuffer();

        // Start final scene render pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CURRENT_FRAME], finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            meshes[i].GetModelMatrix() = glm::rotate(meshes[i].GetModelMatrix(), glm::radians(150 * deltaTime), rotations[i]);
            modelMatrices[i] = meshes[i].GetModelMatrix();
        }
#if defined(INSTANCED_DRAWING_TEST)

        memcpy(mappedInstancedData, modelMatrices.data(), sizeof(glm::mat4) * OBJECT_COUNT);
#elif defined(SEPARATE_DRAW_CALL_TEST) 

        memcpy(mappedModelUBO, modelMatrices.data(), sizeof(glm::mat4) * OBJECT_COUNT);
#endif 
        vkCmdBindPipeline(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVKPipeline());
        vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 0, 1, &meshes[0].GetVBO()->GetVKBuffer(), &offsets);
        vkCmdBindVertexBuffers(cmdBuffers[CURRENT_FRAME], 1, 1, &instancedDataBuffer, &offsets);
        vkCmdBindIndexBuffer(cmdBuffers[CURRENT_FRAME], meshes[0].GetIBO()->GetVKBuffer(), 0, VK_INDEX_TYPE_UINT32);

#if defined(INSTANCED_DRAWING_TEST)

        vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &dscSets[0]->GetVKDescriptorSet(), 0, nullptr);
        vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], meshes[0].GetIBO()->GetIndices().size(), OBJECT_COUNT, 0, 0, 0);
#elif defined(SEPARATE_DRAW_CALL_TEST) 

        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            vkCmdBindDescriptorSets(cmdBuffers[CURRENT_FRAME], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &dscSets[i]->GetVKDescriptorSet(), 0, nullptr);
            vkCmdDrawIndexed(cmdBuffers[CURRENT_FRAME], meshes[0].GetIBO()->GetIndices().size(), 1, 0, 0, 0);
        }
#endif 

        // End the command buffer recording phase.
        CommandBuffer::EndRenderPass(cmdBuffers[CURRENT_FRAME]);
        CommandBuffer::End(cmdBuffers[CURRENT_FRAME]);

        // Submit the command buffer to a queue.
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[CURRENT_FRAME] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffers[CURRENT_FRAME];
        VkSemaphore signalSemaphores[] = { renderingCompleteSemaphores[CURRENT_FRAME] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;


        VkQueue graphicsQueue = s_Device->GetGraphicsQueue();
        ASSERT(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inRenderingFences[CURRENT_FRAME]) == VK_SUCCESS, "Failed to submit draw command buffer!");

        currentTime = GetRenderTime();
        frameCount++;
        if (currentTime - m_LastFrameRenderTime >= 1.0)
        {
            std::cout << frameCount << std::endl;

            frameCount = 0;
            m_LastFrameRenderTime = currentTime;
        }

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

        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), diffuseSampler, nullptr);
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), normalSampler, nullptr);
        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), RMSampler, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), modelMatUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), modelMatUBOBufferMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), projMatUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), projMatUBOBufferMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), viewMatUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), viewMatUBOBufferMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), dirLightPosUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), dirLightPosUBOBufferMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), testFlagUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), testFlagUBOMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), camPosUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), camPosUBOBufferMemory, nullptr);

        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), instancedDataBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), instancedDataMemory, nullptr);

        delete modelLoad;
       
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