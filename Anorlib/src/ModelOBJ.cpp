#define TINYOBJLOADER_IMPLEMENTATION
#include "ModelOBJ.h"
#include <string>
#include "Buffer.h",
#include <memory>
#include "LogicalDevice.h"
#include "PhysicalDevice.h"
#include "DescriptorSet.h"
#include "Containers.h"
#include <unordered_map>
namespace Anor
{
	ModelOBJ::ModelOBJ(const char* OBJPath, const char* texturePath, const Ref<LogicalDevice>& device, const Ref<PhysicalDevice>& physDevice, const Ref<DescriptorSet>& dscSet, uint32_t graphcisQueueIndex)
        :m_PhysicalDevice(physDevice), m_Device(device), m_DescriptorSet(dscSet)
	{
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        ASSERT(tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, OBJPath), warn + err);

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};
        
                vertex.pos =
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord =
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
                vertex.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(m_Vertices.size());
                    m_Vertices.push_back(vertex);
                }

                m_Indices.push_back(uniqueVertices[vertex]);
            }
        }

        m_VBO = std::make_shared<VertexBuffer>(m_Device, m_PhysicalDevice, m_Vertices, graphcisQueueIndex);
        m_IBO = std::make_shared<IndexBuffer>(m_Device, m_PhysicalDevice, m_Indices, graphcisQueueIndex);
        m_UBO = std::make_shared<UniformBuffer>(m_Device, m_PhysicalDevice, sizeof(UniformBufferObject));
        m_ImBO = std::make_shared<ImageBuffer>(m_Device, m_PhysicalDevice, texturePath, graphcisQueueIndex);

        m_UBO->UpdateUniformBuffer(sizeof(UniformBufferObject), m_DescriptorSet);
        m_ImBO->UpdateImageBuffer(m_DescriptorSet);
	}
    ModelOBJ::~ModelOBJ()
    {
    }
}