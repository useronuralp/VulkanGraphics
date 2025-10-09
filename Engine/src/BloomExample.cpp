#include "pch.h"
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
#define MAX_FRAMES_IN_FLIGHT 1
#define BLUR_PASS_COUNT 6
using namespace OVK;
class BloomExample : public OVK::VulkanApplication
{
public:
    BloomExample(uint32_t framesInFlight) : VulkanApplication(framesInFlight) {}
private:
    float m_DeltaTimeLastFrame = GetRenderTime();
    struct ModelUBO
    {
        glm::mat4 viewMatrix;
        glm::mat4 projMatrix;
    };
    // HDR framebuffer variables.
    Ref<Framebuffer>            HDRFramebuffer;
    VkRenderPass                HDRRenderPass;
    Ref<Image>                  HDRColorImage;
    Ref<Image>                  HDRDepthImage;
    VkRenderPassBeginInfo       HDRRenderPassBeginInfo;
    Ref<Pipeline>               finalPassPipeline;
    Ref<DescriptorLayout>       HDRSetLayout;
    std::array<VkClearValue, 2> clearValues;

    Ref<DescriptorLayout> m_OneSamplerLayout;

    VkSampler               finalPassSampler;
    VkDescriptorSet         finalPassDescriptorSet;

    Ref<Bloom>              bloomAgent;

    Ref<DescriptorPool>     pool;
    Ref<Pipeline>           EmissiveObjectPipeline;
    Model*                  model;
    ModelUBO                modelUBO;
    VkBuffer                modelUBOBuffer;
    VkDeviceMemory          modelUBOBufferMemory;

    glm::vec4                   directionalLightPosition = glm::vec4(-10.0f, 35.0f, -22.0f, 1.0f);
    VkRenderPassBeginInfo       finalScenePassBeginInfo;
    VkCommandBuffer             cmdBuffers[MAX_FRAMES_IN_FLIGHT];
    VkCommandPool               cmdPool;

    void* mappedModelUBOBuffer;
    void CreateRenderPasses()
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
    void CreateFramebuffers()
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

    
	void OnVulkanInit()
	{
		SetDeviceExtensions({ VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME });
		SetCameraConfiguration(45.0f, 0.1f, 1500.0f);
	}
	void OnStart()
	{
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
        clearValues[1].depthStencil = { 1.0f, 0 };

        // Order of the following two functions is importante.
        CreateRenderPasses();

        CreateFramebuffers();

		Utils::CreateVKBuffer(sizeof(ModelUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, modelUBOBuffer, modelUBOBufferMemory);
		vkMapMemory(s_Device->GetVKDevice(), modelUBOBufferMemory, 0, sizeof(ModelUBO), 0, &mappedModelUBOBuffer);

		std::vector<DescriptorBindingSpecs> dscLayout
		{
			DescriptorBindingSpecs { Type::UNIFORM_BUFFER, sizeof(ModelUBO), 1, ShaderStage::VERTEX,    0},
		};

		std::vector<VkDescriptorType> types;
		types.push_back(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
		HDRSetLayout = std::make_shared<DescriptorLayout>(dscLayout);
		pool = std::make_shared<DescriptorPool>(200, types);


        // Blur pass params.
        std::vector<DescriptorBindingSpecs> dscLayout2
        {
            DescriptorBindingSpecs { Type::TEXTURE_SAMPLER_DIFFUSE, UINT64_MAX, 1, ShaderStage::FRAGMENT , 0},
        };
        
        types.clear();
        types.push_back(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        m_OneSamplerLayout = std::make_shared<DescriptorLayout>(dscLayout2);

        // Allocate final pass descriptor Set.
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool->GetDescriptorPool();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_OneSamplerLayout->GetDescriptorLayout();

        VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &finalPassDescriptorSet);
        ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");



        Pipeline::Specs specs{};
        specs.DescriptorLayout = m_OneSamplerLayout;
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

        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.resize(0);

        std::vector<VkVertexInputBindingDescription> bindingDescs;
        bindingDescs.clear();
        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;
        
        finalPassPipeline = std::make_shared<Pipeline>(specs);


        // Emissive object pipeline.
        specs.DescriptorLayout = HDRSetLayout;
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
        bindingDescription.stride = sizeof(glm::vec3);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bindingDescs.clear();
        bindingDescs.push_back(bindingDescription);


        attributeDescriptions.resize(1);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = 0;


        specs.pVertexBindingDesc = bindingDescs;
        specs.pVertexAttributeDescriptons = attributeDescriptions;

        EmissiveObjectPipeline = std::make_shared<Pipeline>(specs);

        model = new OVK::Model(std::string(SOLUTION_DIR) + "Engine\\models\\sword\\scene.gltf", LOAD_VERTEX_POSITIONS , pool, HDRSetLayout);
        model->Scale(0.7f, 0.7f, 0.7f);
        model->Rotate(90, 0, 1, 0);

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

            vkUpdateDescriptorSets(s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
        }

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

        glm::mat4 view = s_Camera->GetViewMatrix();
        glm::mat4 proj = s_Camera->GetProjectionMatrix();
        glm::vec4 cameraPos = glm::vec4(s_Camera->GetPosition(), 1.0f);

        modelUBO.viewMatrix = view;
        modelUBO.projMatrix = proj;
        memcpy(mappedModelUBOBuffer, &modelUBO, sizeof(modelUBO));

        glm::mat4 mat = model->GetModelMatrix();
        model->Rotate(10.0f * DeltaTime(), 0, 1, 0);

        clearValues[0] = { 0.2f, 0.1f, 0.15f, 1.0f };

        HDRRenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        HDRRenderPassBeginInfo.framebuffer = HDRFramebuffer->GetVKFramebuffer();
        HDRRenderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        HDRRenderPassBeginInfo.pClearValues = clearValues.data();
        HDRRenderPassBeginInfo.pNext = nullptr;
        HDRRenderPassBeginInfo.renderPass = HDRRenderPass;
        HDRRenderPassBeginInfo.renderArea.offset = { 0, 0 };
        HDRRenderPassBeginInfo.renderArea.extent.height = HDRFramebuffer->GetHeight();
        HDRRenderPassBeginInfo.renderArea.extent.width = HDRFramebuffer->GetWidth();

        struct emissivePushConstans
        {
            glm::mat4 modelMat;
            glm::vec4 tonemap;
        }pc;

        pc.tonemap.x = 0.0f;
        pc.modelMat = mat;
        // Image format : VK_IMAGE_LAYOUT_UNDEFINED.
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], HDRRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, EmissiveObjectPipeline);
        CommandBuffer::PushConstants(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4) + sizeof(glm::vec4), &pc);
        model->DrawIndexed(cmdBuffers[CurrentFrameIndex()], EmissiveObjectPipeline->GetPipelineLayout());
        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);

        VkDeviceSize offset = { 0 };

        bloomAgent->ApplyBloom(cmdBuffers[CurrentFrameIndex()]);


        // Combine bloom with original image pass.


        clearValues[0].color = { {0.18f, 0.18f, 0.7f, 1.0f} };
        
        finalScenePassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        finalScenePassBeginInfo.renderPass = s_Swapchain->GetSwapchainRenderPass();
        finalScenePassBeginInfo.framebuffer = s_Swapchain->GetActiveFramebuffer();
        finalScenePassBeginInfo.renderArea.offset = { 0, 0 };
        finalScenePassBeginInfo.renderArea.extent = s_Surface->GetVKExtent();
        finalScenePassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        finalScenePassBeginInfo.pClearValues = clearValues.data();
        
        // Start final scene render pass.
        CommandBuffer::BeginRenderPass(cmdBuffers[CurrentFrameIndex()], finalScenePassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        CommandBuffer::BindPipeline(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline);
        vkCmdBindDescriptorSets(cmdBuffers[CurrentFrameIndex()], VK_PIPELINE_BIND_POINT_GRAPHICS, finalPassPipeline->GetPipelineLayout(), 0, 1, &finalPassDescriptorSet, 0, nullptr);
        vkCmdDraw(cmdBuffers[CurrentFrameIndex()], 3, 1, 0, 0);
        
        CommandBuffer::EndRenderPass(cmdBuffers[CurrentFrameIndex()]);
        CommandBuffer::EndRecording(cmdBuffers[CurrentFrameIndex()]);

        SubmitCommandBuffer(cmdBuffers[CurrentFrameIndex()]);
	}
	void OnWindowResize()
	{
        finalPassPipeline->OnResize();
        EmissiveObjectPipeline->OnResize();
        vkDestroySampler(s_Device->GetVKDevice(), finalPassSampler, nullptr);

        CreateFramebuffers();
        
        bloomAgent = std::make_shared<Bloom>();
        bloomAgent->ConnectImageResourceToAddBloom(HDRColorImage);

        finalPassSampler = Utils::CreateSampler(bloomAgent->GetPostProcessedImage(), ImageType::COLOR, VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::WriteDescriptorSetWithSampler(finalPassDescriptorSet, finalPassSampler, bloomAgent->GetPostProcessedImage()->GetImageView(), 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	void OnCleanup()
	{
        delete model;

        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            CommandBuffer::FreeCommandBuffer(cmdBuffers[i], cmdPool, s_Device->GetGraphicsQueue());
        }
        CommandBuffer::DestroyCommandPool(cmdPool);
        vkFreeMemory(s_Device->GetVKDevice(), modelUBOBufferMemory, nullptr);
        vkDestroyBuffer(s_Device->GetVKDevice(), modelUBOBuffer, nullptr);

        vkDestroyRenderPass(VulkanApplication::s_Device->GetVKDevice(), HDRRenderPass, nullptr);
        vkDestroySampler(s_Device->GetVKDevice(), finalPassSampler, nullptr);
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
//int main()
//{
//	BloomExample app(MAX_FRAMES_IN_FLIGHT);
//	app.Run();
//	return 0;
//}