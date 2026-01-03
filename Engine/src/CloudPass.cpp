#include "CloudPass.h"
#include "Utils.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
// #define STB_IMAGE_IMPLEMENTATION
#include "CommandBuffer.h"
#include "Device.h"
#include "EngineInternal.h"
#include "PhysicalDevice.h"

#include <stb_image.h>

// Loads a 2D texture (LDR or HDR) for CloudPass
inline CloudTexture LoadCloudTexture(VkDevice device, VkPhysicalDevice physicalDevice, VkQueue transferQueue, const std::string& path, bool generateMips = false)
{
    CloudTexture tex{};

    int texWidth, texHeight, texChannels;
    //stbi_set_flip_vertically_on_load(true);

    bool  isHDR  = stbi_is_hdr(path.c_str()) != 0;
    void* pixels = nullptr;

    if (isHDR)
        pixels = stbi_loadf(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    else
        pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels)
        throw std::runtime_error("Failed to load texture: " + path);

    VkDeviceSize imageSize = texWidth * texHeight * 4 * (isHDR ? sizeof(float) : sizeof(uint8_t));
    tex.width              = texWidth;
    tex.height             = texHeight;

    tex.mipLevels          = generateMips ? static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1 : 1;

    VkFormat format        = isHDR ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;

    // Create VkImage
    VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.format        = format;
    imgInfo.extent        = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | (generateMips ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0);
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    ENSURE(vkCreateImage(device, &imgInfo, nullptr, &tex.image) == VK_SUCCESS, "Failed to create image!");

    // Allocate memory
    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device, tex.image, &memReq);
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.allocationSize  = memReq.size;
    allocInfo.memoryTypeIndex = Utils::FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ENSURE(vkAllocateMemory(device, &allocInfo, nullptr, &tex.memory) == VK_SUCCESS, "Failed to allocate image memory!");
    vkBindImageMemory(device, tex.image, tex.memory, 0);

    // Staging buffer
    VkBuffer       stagingBuffer;
    VkDeviceMemory stagingMem;
    Utils::CreateVKBuffer(
        imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingMem);

    void* data;
    vkMapMemory(device, stagingMem, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingMem);

    stbi_image_free(pixels);

    // Transition image layout
    VkCommandBuffer singleCmdBuffer;
    VkCommandPool   singleCmdPool;
    CommandBuffer::CreateCommandBufferPool(EngineInternal::GetContext()._QueueFamilies.GraphicsFamily, singleCmdPool);
    CommandBuffer::CreateCommandBuffer(singleCmdBuffer, singleCmdPool);
    CommandBuffer::BeginRecording(singleCmdBuffer);

    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = tex.image;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = tex.mipLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(singleCmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer -> image
    VkBufferImageCopy region{};
    region.bufferOffset                    = 0;
    region.bufferRowLength                 = 0;
    region.bufferImageHeight               = 0;
    region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel       = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount     = 1;
    region.imageOffset                     = { 0, 0, 0 };
    region.imageExtent                     = { static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1 };
    vkCmdCopyBufferToImage(singleCmdBuffer, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition image to SHADER_READ_ONLY_OPTIMAL after upload
    VkImageMemoryBarrier postCopyBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    postCopyBarrier.oldLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    postCopyBarrier.newLayout                       = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    postCopyBarrier.srcAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
    postCopyBarrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT;
    postCopyBarrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    postCopyBarrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    postCopyBarrier.image                           = tex.image;
    postCopyBarrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    postCopyBarrier.subresourceRange.baseMipLevel   = 0;
    postCopyBarrier.subresourceRange.levelCount     = tex.mipLevels;
    postCopyBarrier.subresourceRange.baseArrayLayer = 0;
    postCopyBarrier.subresourceRange.layerCount     = 1;

    vkCmdPipelineBarrier(
        singleCmdBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &postCopyBarrier);

    CommandBuffer::EndRecording(singleCmdBuffer);
    CommandBuffer::Submit(singleCmdBuffer, EngineInternal::GetContext().GetDevice()->GetGraphicsQueue());
    CommandBuffer::FreeCommandBuffer(singleCmdBuffer, singleCmdPool, EngineInternal::GetContext().GetDevice()->GetGraphicsQueue());
    CommandBuffer::DestroyCommandPool(singleCmdPool);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMem, nullptr);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter        = VK_FILTER_LINEAR;
    samplerInfo.minFilter        = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.minLod           = 0;
    samplerInfo.maxLod           = static_cast<float>(tex.mipLevels);
    samplerInfo.mipLodBias       = 0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    ENSURE(vkCreateSampler(device, &samplerInfo, nullptr, &tex.sampler) == VK_SUCCESS, "Failed to create sampler!");

    // Create image view
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image                           = tex.image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = format;
    viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = tex.mipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;
    ENSURE(vkCreateImageView(device, &viewInfo, nullptr, &tex.view) == VK_SUCCESS, "Failed to create image view!");

    return tex;
}

// -------------------------------------------------------------------------------------------------
// Utility: Load a SPIR-V shader module from file
// -------------------------------------------------------------------------------------------------
static VkShaderModule LoadShaderModule(VkDevice device, const char* path)
{
    auto code = Utils::ReadFile(std::string(path));

    VkShaderModuleCreateInfo info{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    if (vkCreateShaderModule(device, &info, nullptr, &module) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shader module.");

    return module;
}

// -------------------------------------------------------------------------------------------------
// Destructor
// -------------------------------------------------------------------------------------------------
CloudPass::~CloudPass()
{
    Destroy();
}

// -------------------------------------------------------------------------------------------------
// Create
// -------------------------------------------------------------------------------------------------
void CloudPass::Create(VkDevice inDevice, VkExtent2D inExtent, VkDescriptorPool inDescriptorPool)
{
    device         = inDevice;
    extent         = inExtent;
    descriptorPool = inDescriptorPool;

    CreateSamplers();
    CreateOutputImages();
    CreateNoiseTextures();
    CreatePhaseLUT();
    CreateMultiScatterLUT();
    CreateWeatherMap();
    CreateDescriptorSetLayout();
    AllocateAndWriteDescriptorSet();
    CreatePipelineLayout();
    CreateComputePipeline();
}

// -------------------------------------------------------------------------------------------------
// Destroy
// -------------------------------------------------------------------------------------------------
void CloudPass::Destroy()
{
    if (!device)
        return;

    DestroyPipeline();
    DestroyOutputImages();

    vkDestroyImageView(device, outputColorView, nullptr);
    vkDestroyImageView(device, outputTransmittanceView, nullptr);
    vkDestroyImage(device, outputColorImage, nullptr);
    vkDestroyImage(device, outputTransmittanceImage, nullptr);
    vkFreeMemory(device, outputColorMemory, nullptr);
    vkFreeMemory(device, outputTransmittanceMemory, nullptr);

    vkDestroyImageView(device, baseNoise3D.view, nullptr);
    vkDestroyImageView(device, detailNoise3D.view, nullptr);
    vkDestroyImageView(device, phaseLUT.view, nullptr);
    vkDestroyImageView(device, multiScatterLUT.view, nullptr);
    vkDestroyImageView(device, weatherMap.view, nullptr);

    vkDestroyImage(device, baseNoise3D.image, nullptr);
    vkDestroyImage(device, detailNoise3D.image, nullptr);
    vkDestroyImage(device, phaseLUT.image, nullptr);
    vkDestroyImage(device, multiScatterLUT.image, nullptr);
    vkDestroyImage(device, weatherMap.image, nullptr);

    vkFreeMemory(device, baseNoise3D.memory, nullptr);
    vkFreeMemory(device, detailNoise3D.memory, nullptr);
    vkFreeMemory(device, phaseLUT.memory, nullptr);
    vkFreeMemory(device, multiScatterLUT.memory, nullptr);
    vkFreeMemory(device, weatherMap.memory, nullptr);

    vkDestroySampler(device, sampler2D, nullptr);
    vkDestroySampler(device, sampler3D, nullptr);

    if (descriptorSetLayout)
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    device = VK_NULL_HANDLE;
}

// -------------------------------------------------------------------------------------------------
// Create output color and transmittance images
// -------------------------------------------------------------------------------------------------
void CloudPass::CreateOutputImages()
{
    VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgInfo.imageType   = VK_IMAGE_TYPE_2D;
    imgInfo.format      = VK_FORMAT_R16G16B16A16_SFLOAT;
    imgInfo.extent      = { extent.width, extent.height, 1 };
    imgInfo.mipLevels   = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

    // Color
    vkCreateImage(device, &imgInfo, nullptr, &outputColorImage);
    // Allocate and bind memory (use your allocator)

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, outputColorImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memRequirements.size;
    allocInfo.memoryTypeIndex = Utils::FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ENSURE(vkAllocateMemory(device, &allocInfo, nullptr, &outputColorMemory) == VK_SUCCESS, "Failed to allocate image memory!");

    vkBindImageMemory(device, outputColorImage, outputColorMemory, 0);

    // Transmittance
    ENSURE(vkCreateImage(device, &imgInfo, nullptr, &outputTransmittanceImage) == VK_SUCCESS, "");
    // Allocate and bind memory

    VkMemoryRequirements memRequirements2;
    vkGetImageMemoryRequirements(device, outputTransmittanceImage, &memRequirements2);
    VkMemoryAllocateInfo allocInfo2{};
    allocInfo2.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo2.allocationSize  = memRequirements2.size;
    allocInfo2.memoryTypeIndex = Utils::FindMemoryType(memRequirements2.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    ENSURE(vkAllocateMemory(device, &allocInfo2, nullptr, &outputTransmittanceMemory) == VK_SUCCESS, "Failed to allocate image memory!");

    vkBindImageMemory(device, outputTransmittanceImage, outputTransmittanceMemory, 0);

    // Create Views
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                      = imgInfo.format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    viewInfo.image                       = outputColorImage;
    ENSURE(vkCreateImageView(device, &viewInfo, nullptr, &outputColorView) == VK_SUCCESS, "");
    viewInfo.image = outputTransmittanceImage;
    ENSURE(vkCreateImageView(device, &viewInfo, nullptr, &outputTransmittanceView) == VK_SUCCESS, "");
}

// -------------------------------------------------------------------------------------------------
// Destroy output images
// -------------------------------------------------------------------------------------------------
void CloudPass::DestroyOutputImages()
{
    if (outputColorView)
        vkDestroyImageView(device, outputColorView, nullptr);
    if (outputTransmittanceView)
        vkDestroyImageView(device, outputTransmittanceView, nullptr);
    if (outputColorImage)
        vkDestroyImage(device, outputColorImage, nullptr);
    if (outputTransmittanceImage)
        vkDestroyImage(device, outputTransmittanceImage, nullptr);

    outputColorView = outputTransmittanceView = VK_NULL_HANDLE;
    outputColorImage = outputTransmittanceImage = VK_NULL_HANDLE;
}

// -------------------------------------------------------------------------------------------------
// Descriptor Set Layout
// -------------------------------------------------------------------------------------------------
void CloudPass::CreateDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 7> bindings{};

    bindings[0] = { 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[1] = { 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[2] = { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[3] = { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[4] = { 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[5] = { 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
    bindings[6] = { 6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };

    VkDescriptorSetLayoutCreateInfo info{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    info.bindingCount = static_cast<uint32_t>(bindings.size());
    info.pBindings    = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create CloudPass descriptor set layout.");
}

// -------------------------------------------------------------------------------------------------
// Allocate and write descriptor set
// -------------------------------------------------------------------------------------------------
void CloudPass::AllocateAndWriteDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate CloudPass descriptor set.");

    std::array<VkWriteDescriptorSet, 7> writes{};
    VkDescriptorImageInfo               colorOut{ VK_NULL_HANDLE, outputColorView, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo               transOut{ VK_NULL_HANDLE, outputTransmittanceView, VK_IMAGE_LAYOUT_GENERAL };
    VkDescriptorImageInfo               baseNoise{ sampler3D, baseNoise3D.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo               detailNoise{ sampler3D, detailNoise3D.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo               weather{ sampler2D, weatherMap.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo               phase{ sampler2D, phaseLUT.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    VkDescriptorImageInfo               multiScatter{ sampler2D, multiScatterLUT.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

    writes[0] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &colorOut, nullptr, nullptr };
    writes[1] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &transOut, nullptr, nullptr };
    writes[2] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &baseNoise, nullptr, nullptr };
    writes[3] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 3, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &detailNoise, nullptr, nullptr };
    writes[4] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 4, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &weather, nullptr, nullptr };
    writes[5] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 5, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &phase, nullptr, nullptr };
    writes[6] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 6, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &multiScatter, nullptr, nullptr };

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// -------------------------------------------------------------------------------------------------
// Create Pipeline Layout
// -------------------------------------------------------------------------------------------------
void CloudPass::CreatePipelineLayout()
{
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset     = 0;
    range.size       = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo info{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    info.setLayoutCount         = 1;
    info.pSetLayouts            = &descriptorSetLayout;
    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges    = &range;

    if (vkCreatePipelineLayout(device, &info, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create CloudPass pipeline layout.");
}

// -------------------------------------------------------------------------------------------------
// Create Compute Pipeline
// -------------------------------------------------------------------------------------------------
void CloudPass::CreateComputePipeline()
{
    VkShaderModule shader = LoadShaderModule(device, "assets/shaders/cloudComputeCOMP.spv");

    VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
    stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = shader;
    stage.pName  = "main";

    VkComputePipelineCreateInfo info{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    info.stage  = stage;
    info.layout = pipelineLayout;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("Failed to create CloudPass compute pipeline.");

    vkDestroyShaderModule(device, shader, nullptr);
}

// -------------------------------------------------------------------------------------------------
// Destroy Pipeline
// -------------------------------------------------------------------------------------------------
void CloudPass::DestroyPipeline()
{
    if (pipeline)
        vkDestroyPipeline(device, pipeline, nullptr);
    if (pipelineLayout)
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    pipeline       = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
}

// -------------------------------------------------------------------------------------------------
// Record Commands
// -------------------------------------------------------------------------------------------------
void CloudPass::Record(VkCommandBuffer cmd, const PushConstants& push)
{
    // Barrier to prepare output images
    std::array<VkImageMemoryBarrier, 2> barriers{};
    for (int i = 0; i < 2; ++i)
    {
        barriers[i].sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout                   = VK_IMAGE_LAYOUT_GENERAL;
        barriers[i].srcAccessMask               = 0;
        barriers[i].dstAccessMask               = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[i].subresourceRange.levelCount = 1;
        barriers[i].subresourceRange.layerCount = 1;
        barriers[i].image                       = (i == 0) ? outputColorImage : outputTransmittanceImage;
    }

    vkCmdPipelineBarrier(
        cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, static_cast<uint32_t>(barriers.size()), barriers.data());

    // Bind compute pipeline and descriptors
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstants), &push);

    // Dispatch
    uint32_t groupSizeX = 8;
    uint32_t groupSizeY = 8;
    uint32_t dispatchX  = (extent.width + groupSizeX - 1) / groupSizeX;
    uint32_t dispatchY  = (extent.height + groupSizeY - 1) / groupSizeY;

    vkCmdDispatch(cmd, dispatchX, dispatchY, 1);

    // Barrier for readback in subsequent passes
    for (auto& b : barriers)
    {
        b.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
        b.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    }

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        static_cast<uint32_t>(barriers.size()),
        barriers.data());
}

void CloudPass::CreateSamplers()
{
    VkSamplerCreateInfo info{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    info.magFilter = info.minFilter = VK_FILTER_LINEAR;
    info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.borderColor                                          = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    info.mipmapMode                                           = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.minLod                                               = 0.0f;
    info.maxLod                                               = 16.0f;

    ENSURE(vkCreateSampler(device, &info, nullptr, &sampler2D) == VK_SUCCESS, "Failed to create 2D sampler!");

    info.addressModeU = info.addressModeV = info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    ENSURE(vkCreateSampler(device, &info, nullptr, &sampler3D) == VK_SUCCESS, "Failed to create 3D sampler!");
}

void CloudPass::CreateNoiseTextures()
{
    const VkExtent3D noiseExtent{ 128, 128, 128 };
    VkFormat         noiseFormat = VK_FORMAT_R16_SFLOAT;

    auto createStorage3DImage    = [&](CloudTexture& tex)
    {
        VkImageCreateInfo info{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        info.imageType   = VK_IMAGE_TYPE_3D;
        info.format      = noiseFormat;
        info.extent      = noiseExtent;
        info.mipLevels   = 1;
        info.arrayLayers = 1;
        info.samples     = VK_SAMPLE_COUNT_1_BIT;
        info.tiling      = VK_IMAGE_TILING_OPTIMAL;
        info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        ENSURE(vkCreateImage(device, &info, nullptr, &tex.image) == VK_SUCCESS, "Failed to create 3D noise image");

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(device, tex.image, &memReq);

        VkMemoryAllocateInfo alloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        alloc.allocationSize  = memReq.size;
        alloc.memoryTypeIndex = Utils::FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        ENSURE(vkAllocateMemory(device, &alloc, nullptr, &tex.memory) == VK_SUCCESS, "Failed to allocate noise memory");
        vkBindImageMemory(device, tex.image, tex.memory, 0);

        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image                           = tex.image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.format                          = noiseFormat;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;
        ENSURE(vkCreateImageView(device, &viewInfo, nullptr, &tex.view) == VK_SUCCESS, "Failed to create 3D noise view");
    };

    createStorage3DImage(baseNoise3D);
    createStorage3DImage(detailNoise3D);

    // -----------------------------
    // 3. Generate noise via compute shader
    // -----------------------------
    auto generateNoise = [&](CloudTexture& tex, float seed)
    {
        auto           ctx    = EngineInternal::GetContext();
        VkShaderModule shader = LoadShaderModule(device, "assets/shaders/noise3DCOMP.spv");

        // Descriptor set layout for storage image
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo inf{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        inf.bindingCount = 1;
        inf.pBindings    = &binding;

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(device, &inf, nullptr, &descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool     = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &descriptorSetLayout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);

        VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts    = &descriptorSetLayout;

        // Push constant range for seed
        VkPushConstantRange pcRange{};
        pcRange.stageFlags                = VK_SHADER_STAGE_COMPUTE_BIT;
        pcRange.offset                    = 0;
        pcRange.size                      = sizeof(float);
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pcRange;

        VkPipelineLayout pipelineLayout;
        ENSURE(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout) == VK_SUCCESS, "Failed to create pipeline layout");

        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = shader;
        stage.pName  = "main";

        VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
        pipelineInfo.stage  = stage;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        ENSURE(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) == VK_SUCCESS, "Failed to create compute pipeline");

        // Command buffer
        VkCommandBuffer cmd;
        VkCommandPool   pool;
        CommandBuffer::CreateCommandBufferPool(ctx._QueueFamilies.GraphicsFamily, pool);
        CommandBuffer::CreateCommandBuffer(cmd, pool);
        CommandBuffer::BeginRecording(cmd);

        // Transition image layout to GENERAL
        VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout                   = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask               = 0;
        barrier.dstAccessMask               = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.image                       = tex.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView   = tex.view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = descriptorSet;
        write.dstBinding      = 0;
        write.dstArrayElement = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        // Push the seed
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &seed);

        // Dispatch
        uint32_t groupSize = 8;
        uint32_t dispatchX = (noiseExtent.width + groupSize - 1) / groupSize;
        uint32_t dispatchY = (noiseExtent.height + groupSize - 1) / groupSize;
        uint32_t dispatchZ = (noiseExtent.depth + groupSize - 1) / groupSize;
        vkCmdDispatch(cmd, dispatchX, dispatchY, dispatchZ);

        // Transition image to SHADER_READ_ONLY_OPTIMAL
        VkImageMemoryBarrier postBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        postBarrier.oldLayout                   = VK_IMAGE_LAYOUT_GENERAL;
        postBarrier.newLayout                   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        postBarrier.srcAccessMask               = VK_ACCESS_SHADER_WRITE_BIT;
        postBarrier.dstAccessMask               = VK_ACCESS_SHADER_READ_BIT;
        postBarrier.image                       = tex.image;
        postBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        postBarrier.subresourceRange.levelCount = 1;
        postBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &postBarrier);

        CommandBuffer::EndRecording(cmd);
        CommandBuffer::Submit(cmd, ctx.GetDevice()->GetGraphicsQueue());
        CommandBuffer::FreeCommandBuffer(cmd, pool, ctx.GetDevice()->GetGraphicsQueue());
        CommandBuffer::DestroyCommandPool(pool);

        vkDestroyPipeline(device, pipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyShaderModule(device, shader, nullptr);
    };

    // Use different seeds for base and detail noises
    generateNoise(baseNoise3D, 42.0f);
    generateNoise(detailNoise3D, 1337.0f);
}

void CloudPass::CreatePhaseLUT()
{
    auto ctx = EngineInternal::GetContext();
    phaseLUT = LoadCloudTexture(
        device,
        ctx.GetPhysicalDevice()->GetVKPhysicalDevice(),
        ctx.GetDevice()->GetTransferQueue(),
        (std::string(SOLUTION_DIR) + "Engine/assets/textures/blueNoise.png "));
}

void CloudPass::CreateMultiScatterLUT()
{
    auto ctx        = EngineInternal::GetContext();
    multiScatterLUT = LoadCloudTexture(
        device,
        ctx.GetPhysicalDevice()->GetVKPhysicalDevice(),
        ctx.GetDevice()->GetTransferQueue(),
        (std::string(SOLUTION_DIR) + "Engine/assets/textures/White_Texture.png "));
}

void CloudPass::CreateWeatherMap()
{
    auto ctx   = EngineInternal::GetContext();
    weatherMap = LoadCloudTexture(
        device,
        ctx.GetPhysicalDevice()->GetVKPhysicalDevice(),
        ctx.GetDevice()->GetTransferQueue(),
        (std::string(SOLUTION_DIR) + "Engine/assets/textures/grungeNoise.png "));
}