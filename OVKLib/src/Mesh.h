#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "Buffer.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <map>
#include "Pipeline.h"
namespace OVK
{
	struct Configuration
	{
		std::string						  ConfigurationName;
		Ref<Pipeline>					  Pipeline;
		Ref<DescriptorSet>				  DescriptorSet;
		std::map<int, Ref<UniformBuffer>> UniformBuffers;
		std::vector<VkSampler>		      ImageSamplers;
	};

	class DescriptorSet;
	class Texture;
	class RenderPass;
	class CubemapTexture;
	class Mesh
	{
	public:
		void DrawIndexed(const VkCommandBuffer& cmdBuffer);
		void Draw(const VkCommandBuffer& cmdBuffer);

		Mesh() = default;
		Mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture, const Ref<Texture>& roughnessMetallicTexture, const Ref<Texture>& shadowMap = nullptr);
		Mesh(const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture, const Ref<Texture>& roughnessMetallicTexture, const Ref<Texture>& shadowMap = nullptr);
		Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<CubemapTexture>& cubemapTex);
		Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const std::vector<uint32_t>& indices);
		~Mesh();

		Ref<VertexBuffer> GetVBO() { return m_VBO; }
		Ref<IndexBuffer>  GetIBO() { return m_IBO; }


		Ref<Texture> GetAlbedo() { return m_Albedo; }
		Ref<Texture> GetNormals() { return m_Normals; }
		Ref<Texture> GetRoughnessMetallic() { return m_RoughnessMetallic; }
		Ref<Texture> GetShadowMap() { return m_ShadowMap; }

		void AddConfiguration(const char* configName, Pipeline::Specs pipelineCI, std::vector<DescriptorBindingSpecs> descriptorLayout);
		void AddConfiguration(const char* configName, Ref<Pipeline> pipeline, std::vector<DescriptorBindingSpecs> descriptorLayout);
		void SetActiveConfiguration(const char* configName);
		void SetShadowMap(const Ref<Texture>& shadowMap) { m_ShadowMap = shadowMap; }

		void UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize);
		void OnResize();

		glm::mat4& GetModelMatrix() { return m_ModelMatrix; }
		Ref<DescriptorSet> GetActiveDescriptorSet() { return m_ActiveConfiguration.DescriptorSet; }

		void Rotate(const float& degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);
	private:
		const VkDeviceMemory& FindBufferMemory(uint32_t bufferIndex);
	private:

		Configuration						m_ActiveConfiguration;
		std::vector<Configuration>			m_Configurations;

		uint32_t							m_VertexCount;
		uint32_t							m_IndexCount;

		// Vertex & Index Buffers
		Ref<VertexBuffer>					m_VBO = nullptr;
		Ref<IndexBuffer>					m_IBO = nullptr;

		// PBR textures.
		Ref<Texture>	m_Albedo			= nullptr;
		Ref<Texture>	m_Normals			= nullptr;
		Ref<Texture>	m_RoughnessMetallic = nullptr;
		Ref<Texture>	m_ShadowMap			= nullptr;


		Ref<CubemapTexture>		  m_CubemapTexture = nullptr;
		glm::mat4				  m_ModelMatrix = glm::mat4(1.0f);
	};
}