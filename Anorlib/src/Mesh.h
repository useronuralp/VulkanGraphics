#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "Buffer.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
namespace Anor
{
	class DescriptorSet;
	class Pipeline;
	class Texture;
	class Mesh
	{
	public:
		void Draw(const VkCommandBuffer& cmdBuffer);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Ref<Texture>>& diffuseTextures, const Ref<DescriptorSet>& dscSet);
		Ref<VertexBuffer> GetVBO() { return m_VBO; }
		Ref<IndexBuffer> GetIBO() { return m_IBO; }
		//Ref<UniformBuffer> GetUBO() { return m_UBO; }
		const std::vector<uint32_t>& GetIndices() { return m_Indices; }
		const std::vector<Vertex>& GetVertices() { return m_Vertices; }
		Ref<DescriptorSet> GetDescriptorSet() { return m_DescriptorSet; }
		Ref<Pipeline> GetPipeline() { return m_Pipeline; }
		void UpdateViewProjection(const glm::mat4& viewMat, const glm::mat4& projMat);
		void OnResize();

		void Rotate(const float& degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);
	private:
		std::vector<Vertex>       m_Vertices;
		std::vector<uint32_t>	  m_Indices;

		Ref<VertexBuffer>	m_VBO;
		Ref<IndexBuffer>	m_IBO;

		// You can have many different UBOs here.
		Ref<UniformBuffer>  m_ModelViewProj_UBO;

		std::vector<Ref<ImageBuffer>> m_DiffuseImageBuffers;
		//Texture					  m_SpecularTextureBuffer;
		//Texture					  m_NormalMapBuffer;

		glm::mat4 m_ModelMatrix; 
		Ref<DescriptorSet>  m_DescriptorSet;
		Ref<Pipeline> m_Pipeline;
	};
}