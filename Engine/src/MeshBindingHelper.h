#pragma once
#include "core.h"

class Mesh;
class Model;
class DescriptorPool;
class DescriptorSetLayout;
class Image;

namespace MeshBinding
{
void BindPBR(
    Mesh*                          InMesh,
    Ref<DescriptorPool>            InPool,
    Ref<DescriptorSetLayout>       InLayout,
    VkBuffer                       InGlobalUBO,
    size_t                         InUBOSize,
    Ref<Image>                     InShadowMap,
    const std::vector<Ref<Image>>& InPointShadows);

void BindModelPBR(
    Ref<Model>                     InModel,
    Ref<DescriptorPool>            InPool,
    Ref<DescriptorSetLayout>       InLayout,
    VkBuffer                       InGlobalUBO,
    size_t                         InUBOSize,
    Ref<Image>                     InShadowMap,
    const std::vector<Ref<Image>>& InPointShadows);

void BindSkybox(Mesh* InMesh, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO);

void BindSimple(Mesh* InMesh, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO, size_t InUBOSize);

void BindModelSimple(Ref<Model> InModel, Ref<DescriptorPool> InPool, Ref<DescriptorSetLayout> InLayout, VkBuffer InGlobalUBO, size_t InUBOSize);
} // namespace MeshBinding
