#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "Buffer.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/glm.hpp>
#include <glm/matrix.hpp>
#include <map>
namespace Anor
{
	static size_t FromUsageTypeToSize(UniformUsageType type)
	{
		switch (type)
		{
			case UniformUsageType::MAT4: return sizeof(glm::mat4);
			case UniformUsageType::VEC3: return sizeof(glm::vec3);
		}
	}
	class DescriptorSet;
	class Pipeline;
	class Texture;
	class Mesh
	{
	public:
		void DrawIndexed(const VkCommandBuffer& cmdBuffer);
		void Draw(const VkCommandBuffer& cmdBuffer);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture,
			const Ref<Texture>& roughnessMetallicTexture,const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath);
		Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture,
			const Ref<Texture>& roughnessTexture, const Ref<Texture>& metallicTexture, const Ref<Texture>& AOTexture, const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath);
		Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath);
		~Mesh();
		Ref<VertexBuffer> GetVBO() { return m_VBO; }
		Ref<IndexBuffer> GetIBO() { return m_IBO; }
		const std::vector<uint32_t>& GetIndices() { return m_Indices; }
		const std::vector<Vertex>& GetVertices() { return m_Vertices; }
		Ref<DescriptorSet> GetDescriptorSet() { return m_DescriptorSet; }
		Ref<Pipeline> GetPipeline() { return m_Pipeline; }
		void UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize);
		void OnResize();
		glm::mat4 GetModelMatrix() { return m_ModelMatrix; }

		void Rotate(const float& degree, const float& x, const float& y, const float& z);
		void Translate(const float& x, const float& y, const float& z);
		void Scale(const float& x, const float& y, const float& z);
	private:
		const VkDeviceMemory& FindBufferMemory(uint32_t bufferIndex);
	private:
		std::vector<Vertex>       m_Vertices;
		std::vector<uint32_t>	  m_Indices;


		uint32_t			m_VertexCount;
		Ref<VertexBuffer>	m_VBO;
		Ref<IndexBuffer>	m_IBO;

		// Uniform Buffers.
		std::map<int, Ref<UniformBuffer>>	m_UniformBuffers;
		std::map<int, VkSampler>			m_ImageSamplers;

		glm::mat4 m_ModelMatrix; 
		Ref<DescriptorSet>  m_DescriptorSet;
		Ref<Pipeline> m_Pipeline;
	};
}