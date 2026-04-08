#include "DescriptorSet.h"
#include "Device.h"
#include "EngineInternal.h"
#include "Image.h"
#include "Mesh.h"
#include "MeshBindingHelper.h"
#include "Model.h"
#include "Utils.h"

#include <vulkan/vulkan.h>

namespace MeshBinding
{
static VkDescriptorSet AllocateSet(Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = InPool->GetDescriptorPool();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &InLayout->GetDescriptorLayout();

    VkDescriptorSet set;
    ENSURE(vkAllocateDescriptorSets(EngineInternal::GetContext().GetDevice()->GetVKDevice(), &allocInfo, &set) == VK_SUCCESS, "Failed to allocate descriptor set!");
    return set;
}

void BindPBR(
    Mesh*                          InMesh,
    Ref<DescriptorPool>            InPool,
    Ref<DescriptorSetLayout>       InLayout,
    VkBuffer                       InGlobalUBO,
    size_t                         InUBOSize,
    Ref<Image>                     InShadowMap,
    const std::vector<Ref<Image>>& InPointShadows)
{
    VkDescriptorSet set = AllocateSet(InPool, InLayout);
    InMesh->SetDescriptorSet(set);

    // Binding 0: Global UBO
    Utils::UpdateDescriptorSet(set, InGlobalUBO, 0, InUBOSize, 0);

    // Binding 1: Diffuse
    if (InMesh->GetAlbedo())
    {
        VkSampler diffuseSampler =
            Utils::CreateSampler(InMesh->GetAlbedo(), ImageType::COLOR, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE);
        Utils::UpdateDescriptorSet(set, diffuseSampler, InMesh->GetAlbedo()->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        InMesh->AddSampler(diffuseSampler);
    }

    // Binding 2: Normal
    if (InMesh->GetNormals())
    {
        VkSampler normalSampler =
            Utils::CreateSampler(InMesh->GetNormals(), ImageType::COLOR, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE);
        Utils::UpdateDescriptorSet(set, normalSampler, InMesh->GetNormals()->GetImageView(), 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        InMesh->AddSampler(normalSampler);
    }

    // Binding 3: Roughness/Metallic
    if (InMesh->GetRoughnessMetallic())
    {
        VkSampler rmSampler =
            Utils::CreateSampler(InMesh->GetRoughnessMetallic(), ImageType::COLOR, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_TRUE);
        Utils::UpdateDescriptorSet(set, rmSampler, InMesh->GetRoughnessMetallic()->GetImageView(), 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        InMesh->AddSampler(rmSampler);
    }

    // Binding 4: Directional shadow map
    if (InShadowMap)
    {
        VkSampler shadowSampler =
            Utils::CreateSampler(InShadowMap, ImageType::DEPTH, VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FALSE);
        Utils::UpdateDescriptorSet(set, shadowSampler, InShadowMap->GetImageView(), 4, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        InMesh->AddSampler(shadowSampler);
    }

    // Binding 5: Point shadow cubemaps
    for (int i = 0; i < InPointShadows.size(); i++)
    {
        if (InPointShadows[i])
        {
            VkSampler cubeSampler = Utils::CreateCubemapSampler();
            Utils::UpdateDescriptorSet(set, cubeSampler, InPointShadows[i]->GetImageView(), 5, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, i);
            InMesh->AddSampler(cubeSampler);
        }
    }
}

void BindModelPBR(
    Ref<Model>                     InModel,
    Ref<DescriptorPool>            InPool,
    Ref<DescriptorSetLayout>       InLayout,
    VkBuffer                       InGlobalUBO,
    size_t                         InUBOSize,
    Ref<Image>                     InShadowMap,
    const std::vector<Ref<Image>>& InPointShadows)
{
    for (auto* mesh : InModel->GetMeshes())
    {
        BindPBR(mesh, InPool, InLayout, InGlobalUBO, InUBOSize, InShadowMap, InPointShadows);
    }
}

void BindSkybox(Mesh* InMesh, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO)
{
    VkDescriptorSet set = AllocateSet(InPool, InLayout);
    InMesh->SetDescriptorSet(set);

    // Binding 0: UBO (view matrix)
    Utils::UpdateDescriptorSet(set, InGlobalUBO, sizeof(glm::mat4), sizeof(glm::mat4), 0);

    // Binding 1: Cubemap sampler
    VkSampler sampler = Utils::CreateCubemapSampler();
    Utils::UpdateDescriptorSet(set, sampler, InMesh->GetCubemapTexture()->GetImageView(), 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    InMesh->AddSampler(sampler);
}

void BindSimple(Mesh* InMesh, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO, size_t InUBOSize)
{
    VkDescriptorSet set = AllocateSet(InPool, InLayout);
    InMesh->SetDescriptorSet(set);

    // Binding 0: UBO only
    Utils::UpdateDescriptorSet(set, InGlobalUBO, 0, InUBOSize, 0);
}

void BindModelSimple(Ref<Model> InModel, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO, size_t InUBOSize)
{
    for (auto* mesh : InModel->GetMeshes())
    {
        BindSimple(mesh, InPool, InLayout, InGlobalUBO, InUBOSize);
    }
}
} // namespace MeshBinding