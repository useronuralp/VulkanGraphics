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
	class DescriptorLayout;
	class Texture;
	class CubemapTexture;
	class Mesh
	{
		friend class Model;
	public:
		const VkDescriptorSet&	GetDescriptorSet()		{ return m_DescriptorSet; }
		Ref<Texture>			GetAlbedo()				{ return m_Albedo; }
		Ref<Texture>			GetNormals()			{ return m_Normals; }
		Ref<Texture>			GetRoughnessMetallic()	{ return m_RoughnessMetallic; }
		Ref<Texture>			GetShadowMap()			{ return m_ShadowMap; }
		Ref<CubemapTexture>		GetCubeMap()			{ return m_CubemapTexture; }
		const size_t&			GetVertexCount()		{ return m_Vertices.size(); }
		const size_t&			GetIndexCount()			{ return m_Indices.size(); }

		void SetShadowMap(const Ref<Texture>& shadowMap) { m_ShadowMap = shadowMap; }
	private:
		Mesh() = default;
		Mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture,
			const Ref<Texture>& normalTexture, const Ref<Texture>& roughnessMetallicTexture, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout, const Ref<Texture>& shadowMap = nullptr);
		Mesh(const float* vertices, uint32_t vertexCount, const Ref<CubemapTexture>& cubemapTex, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout);
		~Mesh();
	private:
		VkDescriptorSet			m_DescriptorSet;
		std::vector<VkSampler>	m_Samplers;

		// Raw vertices and indices stored in a continues memory. |_Vertex1(pos-normal-tangent-bitangent)__V2__V3__V4__...__Vn__|
		std::vector<float>		m_Vertices;
		std::vector<uint32_t>	m_Indices;

		// PBR textures.
		Ref<Texture>		m_Albedo			= nullptr;
		Ref<Texture>		m_Normals			= nullptr;
		Ref<Texture>		m_RoughnessMetallic = nullptr;
		Ref<Texture>		m_ShadowMap			= nullptr;
		// Cubemap texture, in case a mesh is created as a cubemap.
		Ref<CubemapTexture>	m_CubemapTexture = nullptr;
	};
}