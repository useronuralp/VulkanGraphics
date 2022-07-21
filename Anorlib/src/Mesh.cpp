#include "Mesh.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include "CommandBuffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Texture.h"

namespace Anor
{
	void Mesh::Draw(const VkCommandBuffer& cmdBuffer)
	{
		// Record the draw command into the "cmdBuffer" parameter.
		CommandBuffer::BindPipeline(cmdBuffer, m_Pipeline, 0);
		CommandBuffer::BindDescriptorSet(cmdBuffer, m_Pipeline->GetPipelineLayout(), m_DescriptorSet);
		CommandBuffer::BindVertexBuffer(cmdBuffer, m_VBO->GetVKBuffer(), 0);
		CommandBuffer::BindIndexBuffer(cmdBuffer, m_IBO->GetVKBuffer());
		CommandBuffer::DrawIndexed(cmdBuffer, static_cast<uint32_t>(m_Indices.size()));
	}

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::vector<Ref<Texture>>& diffuseTextures, const Ref<DescriptorSet>& dscSet)
		:m_Vertices(vertices), m_Indices(indices), m_DescriptorSet(dscSet), m_ModelMatrix(glm::mat4(1.0f))
	{
		m_Pipeline = std::make_shared<Pipeline>(m_DescriptorSet);
		// Create the VBO and IBOs. Every model will have these without exceptions.
		m_VBO = std::make_shared<VertexBuffer>(m_Vertices);
		m_IBO = std::make_shared<IndexBuffer>(m_Indices);

		// Every model also should have a MVP matrix.
		UniformBufferSpecs specs{};
		specs.BufferSize = sizeof(UniformBufferObject_MVP);
		specs.DescriptorSet = m_DescriptorSet;
		specs.BindingIndex = 0; // Bind it to binding number 0 in the shader. This is important.

		m_ModelViewProj_UBO = std::make_shared<UniformBuffer>(specs);

		ImageBufferSpecs imageSpecs{};
		imageSpecs.Texture = diffuseTextures[0]; //TO DO: Using only the first texture inside the vector. This could be problematic.
		imageSpecs.BindingIndex = 1; // TO DO: This needs to change. Needs incrementing.
		imageSpecs.DescriptorSet = m_DescriptorSet;
		
		Ref<ImageBuffer> imBO = std::make_shared<ImageBuffer>(imageSpecs);
		m_DiffuseImageBuffers.push_back(imBO);
	}
	void Mesh::UpdateViewProjection(const glm::mat4& viewMat, const glm::mat4& projMat)
	{
		UniformBufferObject_MVP modelAttribs{};
		modelAttribs.model = m_ModelMatrix;
		modelAttribs.proj = projMat;
		modelAttribs.view = viewMat;

		void* data;
		vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), m_ModelViewProj_UBO->GetBufferMemory(), 0, sizeof(modelAttribs), 0, &data);
		memcpy(data, &modelAttribs, sizeof(modelAttribs));
		vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), m_ModelViewProj_UBO->GetBufferMemory());
	}
	void Mesh::OnResize()
	{
		m_Pipeline->OnResize();
	}
	void Mesh::Rotate(const float& degree, const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::rotate(m_ModelMatrix, glm::radians(degree), glm::vec3(x, y, z));
	}
	void Mesh::Translate(const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::translate(m_ModelMatrix, glm::vec3(x, y, z));
	}
	void Mesh::Scale(const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::scale(m_ModelMatrix, glm::vec3(x, y, z));
	}
}
