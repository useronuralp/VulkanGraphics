#include "Buffer.h"
#include "CommandBuffer.h"
#include "DescriptorSet.h"
#include "Engine.h"
#include "Image.h"
#include "LogicalDevice.h"
#include "Mesh.h"
#include "Model.h"
#include "Utils.h"
#include "VulkanContext.h"
Mesh::Mesh(
    const std::vector<float>&    vertices,
    const std::vector<uint32_t>& indices,
    const Ref<Image>&            diffuseTexture,
    const Ref<Image>&            normalTexture,
    const Ref<Image>&            roughnessMetallicTexture,
    Ref<DescriptorPool>          pool,
    Ref<DescriptorSetLayout>     layout,
    const Ref<Image>&            shadowMap,
    std::vector<Ref<Image>>      pointShadows)
    : m_Albedo(diffuseTexture),
      m_Normals(normalTexture),
      m_RoughnessMetallic(roughnessMetallicTexture),
      m_ShadowMap(shadowMap),
      m_Vertices(vertices),
      m_Indices(indices),
      m_PointShadows(pointShadows)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout->GetDescriptorLayout();

    VkResult rslt = vkAllocateDescriptorSets(Engine::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_DescriptorSet);
    ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    for (const auto& bindingSpecs : layout->GetBindingSpecs()) {
        VkSampler sampler;
        if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_NORMAL) {
            sampler = Utils::CreateSampler(
                m_Normals, ImageType::COLOR, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE);
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_Normals->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        } else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_DIFFUSE) {
            sampler = Utils::CreateSampler(
                m_Albedo, ImageType::COLOR, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE);
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_Albedo->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        } else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC) {
            sampler = Utils::CreateSampler(
                m_RoughnessMetallic,
                ImageType::COLOR,
                VK_FILTER_NEAREST,
                VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_TRUE);
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_RoughnessMetallic->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        } else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_SHADOWMAP) {
            sampler = Utils::CreateSampler(
                m_ShadowMap,
                ImageType::DEPTH,
                VK_FILTER_NEAREST,
                VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                VK_FALSE);
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_ShadowMap->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        } else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_POINTSHADOWMAP) {
            for (int i = 0; i < m_PointShadows.size(); i++) {
                sampler = Utils::CreateCubemapSampler();
                Utils::UpdateDescriptorSet(
                    m_DescriptorSet,
                    sampler,
                    m_PointShadows[i]->GetImageView(),
                    bindingSpecs.Binding,
                    VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                    i);
                m_Samplers.push_back(sampler);
            }
        }
    }
}

Mesh::Mesh(
    const float*             vertices,
    uint32_t                 vertexCount,
    const Ref<Image>&        cubemapTex,
    Ref<DescriptorPool>      pool,
    Ref<DescriptorSetLayout> layout)
    : m_CubemapTexture(cubemapTex)
{
    // Fill up the vector.
    for (int i = 0; i < vertexCount; i++) {
        m_Vertices.push_back(vertices[i]);
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = pool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &layout->GetDescriptorLayout();

    VkResult rslt = vkAllocateDescriptorSets(Engine::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &m_DescriptorSet);
    ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

    for (const auto& bindingSpecs : layout->GetBindingSpecs()) {
        VkSampler sampler;
        if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_CUBEMAP) {
            sampler = Utils::CreateCubemapSampler();
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_CubemapTexture->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        }
        /*This constructor was only reserved for cubemap creations but I figured
        I'd support cubes with the same texture applied to all faces here as
        well. I am storing that single texture in m_CubemapTexture variable as
        well. It could cause confusion. change the name of m_CubemapTexture
        later maybe?*/
        else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_DIFFUSE) {
            sampler = Utils::CreateSampler(
                m_CubemapTexture,
                ImageType::COLOR,
                VK_FILTER_NEAREST,
                VK_FILTER_NEAREST,
                VK_SAMPLER_ADDRESS_MODE_REPEAT,
                VK_TRUE);
            Utils::UpdateDescriptorSet(
                m_DescriptorSet,
                sampler,
                m_CubemapTexture->GetImageView(),
                bindingSpecs.Binding,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            m_Samplers.push_back(sampler);
        }
    }
}

Mesh::~Mesh()
{
    for (const auto& sampler : m_Samplers) {
        vkDestroySampler(Engine::GetContext().GetDevice()->GetVKDevice(), sampler, nullptr);
    }
}
