#pragma once
#include "core.h"

#include <vulkan/vulkan.h>

class Image;
class Mesh
{
   public:
    const std::vector<float>&    GetVertices() const;
    const std::vector<uint32_t>& GetIndices() const;

    size_t GetVertexCount() const;
    size_t GetIndexCount() const;

    // Texture access
    Ref<Image> GetAlbedo() const;
    Ref<Image> GetNormals() const;
    Ref<Image> GetRoughnessMetallic() const;
    Ref<Image> GetCubemapTexture() const;

    // Descriptor set — created externally by MeshBinding
    void                   SetDescriptorSet(VkDescriptorSet InSet);
    const VkDescriptorSet& GetDescriptorSet() const;

    // Samplers — created externally, stored here for lifetime management
    void                          AddSampler(VkSampler InSampler);
    const std::vector<VkSampler>& GetSamplers() const;

   private:
    Mesh() = default;

    Mesh(
        const std::vector<float>&    InVertices,
        const std::vector<uint32_t>& InIndices,
        const Ref<Image>&            InDiffuse,
        const Ref<Image>&            InNormal,
        const Ref<Image>&            InRoughnessMetallic);

    Mesh(const float* InVertices, uint32_t InVertexCount, const Ref<Image>& InTexture = nullptr);

    ~Mesh();

    VkDescriptorSet        _DescriptorSet = VK_NULL_HANDLE;
    std::vector<VkSampler> _Samplers;

    std::vector<float>    _Vertices;
    std::vector<uint32_t> _Indices;

    Ref<Image> _Albedo            = nullptr;
    Ref<Image> _Normals           = nullptr;
    Ref<Image> _RoughnessMetallic = nullptr;
    Ref<Image> _CubemapTexture    = nullptr;

    friend class Model;
};