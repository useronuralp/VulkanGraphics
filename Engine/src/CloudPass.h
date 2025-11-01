#pragma once
#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

// -------------------------------------------------------------------------------------------------
// CloudPass
// -------------------------------------------------------------------------------------------------
// This class encapsulates the resources and logic for a volumetric cloud compute render pass.
// It manages descriptor sets, pipelines, images, and recording dispatch commands.
// Assumes the Vulkan device, allocator, and descriptor pool are already set up externally.
// -------------------------------------------------------------------------------------------------

struct CloudTexture
{
    VkImage        image     = VK_NULL_HANDLE;
    VkDeviceMemory memory    = VK_NULL_HANDLE;
    VkImageView    view      = VK_NULL_HANDLE;
    VkSampler      sampler   = VK_NULL_HANDLE;
    uint32_t       width     = 0;
    uint32_t       height    = 0;
    uint32_t       mipLevels = 1;
};

class CloudPass
{
   public:
    CloudPass() = default;
    ~CloudPass();

    // ------------------------------------------------------------------------------
    // Initialization
    // ------------------------------------------------------------------------------
    void Create(VkDevice device, VkExtent2D extent, VkDescriptorPool descriptorPool);

    // Cleanup all allocated Vulkan resources
    void Destroy();

    // ------------------------------------------------------------------------------
    // Per-frame recording into command buffer
    // ------------------------------------------------------------------------------
    struct PushConstants
    {
        glm::mat4 invViewProj;
        glm::vec3 cameraPos;
        float     time;
        glm::vec3 lightDir;
        int       frameCount;
        float     pad; // Alignment padding
    };

    void Record(VkCommandBuffer cmd, const PushConstants& pushConstants);

    // ------------------------------------------------------------------------------
    // Accessors
    // ------------------------------------------------------------------------------
    inline VkImageView GetOutputColorView() const
    {
        return outputColorView;
    }
    inline VkImageView GetOutputTransmittanceView() const
    {
        return outputTransmittanceView;
    }

    inline VkImage GetOutputColorImage() const
    {
        return outputColorImage;
    }

    inline VkImage GetOutputTransmittanceImage() const
    {
        return outputTransmittanceImage;
    }

    inline VkDescriptorSetLayout GetDescriptorSetLayout() const
    {
        return descriptorSetLayout;
    }

   private:
    // ------------------------------------------------------------------------------
    // Vulkan handles
    // ------------------------------------------------------------------------------
    VkDevice   device = VK_NULL_HANDLE;
    VkExtent2D extent{};

    // Output targets
    VkImage        outputColorImage          = VK_NULL_HANDLE;
    VkImage        outputTransmittanceImage  = VK_NULL_HANDLE;
    VkImageView    outputColorView           = VK_NULL_HANDLE;
    VkImageView    outputTransmittanceView   = VK_NULL_HANDLE;
    VkDeviceMemory outputColorMemory         = VK_NULL_HANDLE;
    VkDeviceMemory outputTransmittanceMemory = VK_NULL_HANDLE;

    // Compute pipeline objects
    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;

    // External sampler handles
    VkSampler sampler2D = VK_NULL_HANDLE;
    VkSampler sampler3D = VK_NULL_HANDLE;

    // Input textures / LUTs (image views only)
    CloudTexture weatherMap;
    CloudTexture phaseLUT;
    CloudTexture multiScatterLUT;
    CloudTexture baseNoise3D; // generated on GPU
    CloudTexture detailNoise3D; // generated on GPU

    // Descriptor pool for allocation
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // ------------------------------------------------------------------------------
    // Internal helpers
    // ------------------------------------------------------------------------------
    void CreateOutputImages();
    void CreateDescriptorSetLayout();
    void AllocateAndWriteDescriptorSet();
    void CreatePipelineLayout();
    void CreateComputePipeline();
    void DestroyOutputImages();
    void DestroyPipeline();
    void CreateNoiseTextures();
    void CreatePhaseLUT();
    void CreateMultiScatterLUT();
    void CreateSamplers();
    void CreateWeatherMap();
};
