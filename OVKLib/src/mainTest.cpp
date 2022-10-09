// Native code
#include "VulkanApplication.h"
#include "Model.h"
#include "Framebuffer.h"
#include "Surface.h"
#include "Swapchain.h"
#include "LogicalDevice.h"
#include "Utils.h"
#include "Window.h"
#include "CommandBuffer.h"
#include "Camera.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include "Model.h"
#include "Mesh.h"
#include "Buffer.h"
// External libs
#include <random>
// -----------------------------------------------------
// -----------------------------------------------------
/*
*   Test #1: define "SEPARATE_DRAW_CALL_TEST" 
*   Test #2: define "INSTANCED_DRAWING_TEST"
*/
#define INSTANCED_DRAWING_TEST
#define OBJECT_COUNT 50000
// -----------------------------------------------------
// -----------------------------------------------------
using namespace OVK;
struct SausageUniformBuffer
{
    glm::mat4 viewMat;
    glm::mat4 projMat;
    glm::vec4 dirLightPos;
    glm::vec4 cameraPos;
};
class MyApplication : public OVK::VulkanApplication
{
public:
    MyApplication(uint32_t framesInFlight) : VulkanApplication(framesInFlight) {}
    // Vulkan Objects
    VkClearValue            depthPassClearValue;
    VkRenderPassBeginInfo   finalScenePassBeginInfo;
    VkCommandBuffer         cmdBuffer;
    VkCommandPool           cmdPool;
    VkCommandPool           commandPool;
    VkDeviceSize            offsets;
    VkSampler               diffuseSampler;

    Ref<DescriptorPool>   pool;
    Ref<DescriptorLayout> layout;

    SausageUniformBuffer    sausageUBO;
    // Buffers
    VkBuffer                sausageUBOBuffer;
    VkDeviceMemory          sausageUBOBufferMemory;
    void*                   mappedSausageUBOBuffer;

    VkBuffer                instancedModelMatrixBuffer;
    VkDeviceMemory          instancedModelMatrixBufferMemory;
    void*                   mappedInstancedModelMatrxiBuffer;

    // Rest of the variables
    std::vector<DescriptorBindingSpecs> dscLayout;
    std::default_random_engine rndEngine;

    Model*  sausageModel;

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
    Ref<DescriptorSet>      dscSet;

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
        std::fill(modelMatrices.begin(), modelMatrices.end(), glm::mat4(1.0));
        rotations.resize(OBJECT_COUNT);
        positions.resize(OBJECT_COUNT);

        offsets = 0;
        // Specify the layout of the descriptor set that you'll use in your shaders. (SAME FOR ALL THE OBJECTS) 
        dscLayout =
        {
            DescriptorBindingSpecs { Type::UNIFORM_BUFFER,          sizeof(SausageUniformBuffer),      1, ShaderStage::VERTEX   , 0},
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX /* Max means no size*/, 1, ShaderStage::FRAGMENT , 1},
        };

        layout = std::make_shared<DescriptorLayout>(dscLayout);

        std::vector<VkDescriptorType> types;
        types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        pool = std::make_shared<DescriptorPool>(200, types);


        // Single descriptor set used for every object.
        dscSet = std::make_shared<DescriptorSet>(dscLayout);

        sausageModel   = new Model(std::string(SOLUTION_DIR) + "OVKLib\\models\\sausage\\scene.gltf", LOAD_VERTICES | LOAD_NORMALS | LOAD_BITANGENT | LOAD_TANGENT | LOAD_UV, pool, layout);
        diffuse     = sausageModel->GetMeshes()[0]->GetAlbedo();
        normal      = sausageModel->GetMeshes()[0]->GetNormals();
        RM          = sausageModel->GetMeshes()[0]->GetRoughnessMetallic();
        shadowMap   = nullptr;

        // Pipeline--------------------------------------------
        Pipeline::Specs specs{};
        specs.DescriptorLayout = layout;
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

        size_t bufferSize = sizeof(SausageUniformBuffer);
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sausageUBOBuffer, sausageUBOBufferMemory);
        // Instanced data buffer that changes every frame.
        bufferSize = sizeof(glm::mat4) * OBJECT_COUNT;
        Utils::CreateVKBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instancedModelMatrixBuffer, instancedModelMatrixBufferMemory);

        // Sampler creations
        Utils::CreateSampler(diffuse, diffuseSampler);
       
        // Write descriptor sets of every single object.
        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            // Create a sossig.
            //meshes[i] = Mesh(diffuse, normal, RM, shadowMap);
            modelMatrices[i] = glm::scale(modelMatrices[i], glm::vec3(0.03f, 0.03f, 0.03f));
            //meshes[i].GetModelMatrix() = glm::scale(meshes[i].GetModelMatrix(), glm::vec3(0.03f, 0.03f, 0.03f));

            rotations[i] = glm::vec3(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f));
            positions[i] = glm::vec3(300 * rnd(-1.0f, 1.0f), 300 * rnd(-1.0f, 1.0f), 300 * rnd(-1.0f, 1.0f));

            modelMatrices[i] = glm::translate(modelMatrices[i], positions[i]);
            modelMatrices[i] = glm::rotate(modelMatrices[i], glm::radians(70.0f), glm::vec3(rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f), rnd(-1.0f, 1.0f)));
        }
      
        // Write the descriptor set.
        VkWriteDescriptorSet descriptorWrite{};
        VkDescriptorBufferInfo bufferInfo{};
        VkDescriptorImageInfo imageInfo{};

        bufferInfo.buffer = sausageUBOBuffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SausageUniformBuffer);

        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = 0;
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
        descriptorWrite.dstSet = dscSet->GetVKDescriptorSet();
        descriptorWrite.dstBinding = 1;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);


        CommandBuffer::CreateCommandPool(s_GraphicsQueueFamily, cmdPool);
        CommandBuffer::CreateCommandBuffer(cmdBuffer, cmdPool);

        // Map the buffers e want to update each frame. (All in this case)
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), sausageUBOBufferMemory, 0, sizeof(SausageUniformBuffer), 0, &mappedSausageUBOBuffer);
        vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), instancedModelMatrixBufferMemory, 0, sizeof(glm::mat4) * OBJECT_COUNT, 0, &mappedInstancedModelMatrxiBuffer);

        sausageUBO.dirLightPos = directionalLightPosition;
    }
    void OnUpdate()
    {
        float currentTime, deltaTime;
        deltaTime = DeltaTime();

        // Begin command buffer recording.
        CommandBuffer::BeginRecording(cmdBuffer);

        sausageUBO.viewMat = s_Camera->GetViewMatrix();
        sausageUBO.projMat = s_Camera->GetProjectionMatrix();
        sausageUBO.cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);
        memcpy(mappedSausageUBOBuffer, &sausageUBO, sizeof(sausageUBO));
        
        // Setup the actual scene render pass here.
        scenePassClearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };
        scenePassClearValues[1].depthStencil = { 1.0f, 0 };

        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetActiveFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = VulkanApplication::s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(scenePassClearValues.size());
        finalScenePassBeginInfo.pClearValues = scenePassClearValues.data();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetFramebuffers()[GetActiveImageIndex()]->GetVKFramebuffer();

        // Start final scene render pass.
        CommandBuffer::BeginRenderPass(cmdBuffer, finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            modelMatrices[i] = glm::rotate(modelMatrices[i], glm::radians(150 * deltaTime), rotations[i]);
            //modelMatrices[i] = meshes[i].GetModelMatrix();
        }
        memcpy(mappedInstancedModelMatrxiBuffer, modelMatrices.data(), sizeof(glm::mat4) * OBJECT_COUNT);


        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetVKPipeline());
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &sausageModel->GetMeshes()[0]->GetVBO()->GetVKBuffer(), &offsets);
        vkCmdBindVertexBuffers(cmdBuffer, 1, 1, &instancedModelMatrixBuffer, &offsets);
        vkCmdBindIndexBuffer(cmdBuffer, sausageModel->GetMeshes()[0]->GetIBO()->GetVKBuffer(), 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, &dscSet->GetVKDescriptorSet(), 0, nullptr);

#if defined(INSTANCED_DRAWING_TEST)

        vkCmdDrawIndexed(cmdBuffer, sausageModel->GetMeshes()[0]->GetIBO()->GetIndices().size(), OBJECT_COUNT, 0, 0, 0);
#elif defined(SEPARATE_DRAW_CALL_TEST) 

        for (int i = 0; i < OBJECT_COUNT; i++)
        {
            vkCmdDrawIndexed(cmdBuffer, sausageModel->GetMeshes()[0]->GetIBO()->GetIndices().size(), 1, 0, 0, i);
        }
#endif 

        // End the command buffer recording phase.
        CommandBuffer::EndRenderPass(cmdBuffer);
        CommandBuffer::EndRecording(cmdBuffer);

        SubmitCommandBuffer(cmdBuffer);
    }
    void OnCleanup()
    {

        vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), diffuseSampler, nullptr);    
        
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), sausageUBOBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), sausageUBOBufferMemory, nullptr);
        vkDestroyBuffer(VulkanApplication::s_Device->GetVKDevice(), instancedModelMatrixBuffer, nullptr);
        vkFreeMemory(VulkanApplication::s_Device->GetVKDevice(), instancedModelMatrixBufferMemory, nullptr);

        delete sausageModel;
       
        CommandBuffer::FreeCommandBuffer(cmdBuffer, cmdPool, s_Device->GetGraphicsQueue());
        CommandBuffer::DestroyCommandPool(cmdPool);
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
    MyApplication app(1);
    app.Run();
    return 0;
}