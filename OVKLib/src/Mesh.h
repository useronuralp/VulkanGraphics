#pragma once
#include "core.h"
// External
#include <vector>
#include <vulkan/vulkan.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <map>
namespace OVK
{
	class IndexBuffer;
	class VertexBuffer;
	class DescriptorPool;
	class DescriptorSetLayout;
	class Image;
	class CubemapTexture;
	class Mesh
	{
		friend class Model;
	public:
		const VkDescriptorSet&	GetDescriptorSet()		{ return m_DescriptorSet; }
		Ref<Image>				GetAlbedo()				{ return m_Albedo; }
		Ref<Image>				GetNormals()			{ return m_Normals; }
		Ref<Image>				GetRoughnessMetallic()	{ return m_RoughnessMetallic; }
		Ref<Image>				GetShadowMap()			{ return m_ShadowMap; }
		Ref<Image>				GetCubeMap()			{ return m_CubemapTexture; }
		const size_t&			GetVertexCount()		{ return m_Vertices.size(); }
		const size_t&			GetIndexCount()			{ return m_Indices.size(); }
	private:
		Mesh() = default;
		Mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const Ref<Image>& diffuseTexture,
			const Ref<Image>& normalTexture, const Ref<Image>& roughnessMetallicTexture, Ref<DescriptorPool> pool, Ref<DescriptorSetLayout> layout, const Ref<Image>& shadowMap = nullptr, std::vector<Ref<Image>> pointShadows = std::vector<Ref<Image>>());
		Mesh(const float* vertices, uint32_t vertexCount, const Ref<Image>& cubemapTex, Ref<DescriptorPool> pool, Ref<DescriptorSetLayout> layout);
		~Mesh();
	private:
		VkDescriptorSet			m_DescriptorSet;
		std::vector<VkSampler>	m_Samplers;

		// Raw vertices and indices stored in a continues memory. |_Vertex1(pos-normal-tangent-bitangent)__V2__V3__V4__...__Vn__|
		std::vector<float>		m_Vertices;
		std::vector<uint32_t>	m_Indices;

		// PBR textures.
		Ref<Image>		m_Albedo			= nullptr;
		Ref<Image>		m_Normals			= nullptr;
		Ref<Image>		m_RoughnessMetallic = nullptr;
		Ref<Image>		m_ShadowMap			= nullptr;
		// Cubemap texture, in case a mesh is created as a cubemap.
		Ref<Image>	m_CubemapTexture = nullptr;
		std::vector<Ref<Image>> m_PointShadows;
	};
}