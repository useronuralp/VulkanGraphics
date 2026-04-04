#include "Device.h"
#include "EngineInternal.h"
#include "Image.h"
#include "Mesh.h"

Mesh::Mesh(
    const std::vector<float>&    InVertices,
    const std::vector<uint32_t>& InIndices,
    const Ref<Image>&            InDiffuse,
    const Ref<Image>&            InNormal,
    const Ref<Image>&            InRoughnessMetallic)
    : _Vertices(InVertices), _Indices(InIndices), _Albedo(InDiffuse), _Normals(InNormal), _RoughnessMetallic(InRoughnessMetallic)
{
}

Mesh::Mesh(const float* InVertices, uint32_t InVertexCount, const Ref<Image>& InTexture) : _CubemapTexture(InTexture)
{
    for (uint32_t i = 0; i < InVertexCount; i++)
    {
        _Vertices.push_back(InVertices[i]);
    }
}

Mesh::~Mesh()
{
    for (const auto& sampler : _Samplers)
    {
        vkDestroySampler(EngineInternal::GetContext().GetDevice()->GetVKDevice(), sampler, nullptr);
    }
}

const std::vector<float>& Mesh::GetVertices() const
{
    return _Vertices;
}

const std::vector<uint32_t>& Mesh::GetIndices() const
{
    return _Indices;
}

size_t Mesh::GetVertexCount() const
{
    return _Vertices.size();
}

size_t Mesh::GetIndexCount() const
{
    return _Indices.size();
}

Ref<Image> Mesh::GetAlbedo() const
{
    return _Albedo;
}

Ref<Image> Mesh::GetNormals() const
{
    return _Normals;
}

Ref<Image> Mesh::GetRoughnessMetallic() const
{
    return _RoughnessMetallic;
}

Ref<Image> Mesh::GetCubemapTexture() const
{
    return _CubemapTexture;
}

void Mesh::SetDescriptorSet(VkDescriptorSet InSet)
{
    _DescriptorSet = InSet;
}

const VkDescriptorSet& Mesh::GetDescriptorSet() const
{
    return _DescriptorSet;
}

void Mesh::AddSampler(VkSampler InSampler)
{
    _Samplers.push_back(InSampler);
}

const std::vector<VkSampler>& Mesh::GetSamplers() const
{
    return _Samplers;
}
