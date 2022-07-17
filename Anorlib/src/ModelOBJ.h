#pragma once

#include <tiny_obj_loader.h>
#include "core.h"
#include <vector>
namespace Anor
{
	class PhysicalDevice;
	class LogicalDevice;
	class DescriptorSet;
	class VertexBuffer;
	class IndexBuffer;
	class UniformBuffer;
	class ImageBuffer;
	struct Vertex;
	class ModelOBJ
	{
	public:
		std::vector<Vertex> GetVertices() { return m_Vertices; }
		std::vector<uint32_t> GetIndices() { return m_Indices; }
		Ref<VertexBuffer> GetVBO() { return m_VBO; }
		Ref<IndexBuffer> GetIBO() { return m_IBO; }
		Ref<UniformBuffer> GetUBO() { return m_UBO; }
		Ref<ImageBuffer> GetImBO() { return m_ImBO; }
		ModelOBJ(const char* OBJPath, const char* texturePath, const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physDevice, const Ref<DescriptorSet>& dscSet, uint32_t graphcisQueueIndex);
		~ModelOBJ();
	private:
		std::vector<Vertex> m_Vertices;
		std::vector<uint32_t> m_Indices;

		Ref<LogicalDevice> m_Device;
		Ref<PhysicalDevice> m_PhysicalDevice;
		Ref<DescriptorSet> m_DescriptorSet;

		Ref<VertexBuffer>	m_VBO;
		Ref<IndexBuffer>	m_IBO;
		Ref<UniformBuffer>	m_UBO;
		Ref<ImageBuffer>	m_ImBO;
	};
}