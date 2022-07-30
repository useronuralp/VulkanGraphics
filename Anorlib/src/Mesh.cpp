#include "Mesh.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include "CommandBuffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Texture.h"

namespace Anor
{
	void Mesh::DrawIndexed(const VkCommandBuffer& cmdBuffer)
	{
		CommandBuffer::BindPipeline(cmdBuffer, m_Pipeline, 0);
		// TO DO (Optimiziation): You should bind the a descriptor set only once for objects that use the same descriptor set. 
		CommandBuffer::BindDescriptorSet(cmdBuffer, m_Pipeline->GetPipelineLayout(), m_DescriptorSet);
		CommandBuffer::BindVertexBuffer(cmdBuffer, m_VBO->GetVKBuffer(), 0);
		CommandBuffer::BindIndexBuffer(cmdBuffer, m_IBO->GetVKBuffer());
		CommandBuffer::DrawIndexed(cmdBuffer, static_cast<uint32_t>(m_Indices.size()));
	}
	void Mesh::Draw(const VkCommandBuffer& cmdBuffer)
	{
		CommandBuffer::BindPipeline(cmdBuffer, m_Pipeline, 0);
		CommandBuffer::BindDescriptorSet(cmdBuffer, m_Pipeline->GetPipelineLayout(), m_DescriptorSet);
		CommandBuffer::BindVertexBuffer(cmdBuffer, m_VBO->GetVKBuffer(), 0);
		CommandBuffer::Draw(cmdBuffer, m_VertexCount);
	}
	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture,
		const Ref<Texture>& roughnessMetallicTexture, const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath)
		:m_Vertices(vertices), m_Indices(indices), m_DescriptorSet(dscSet), m_ModelMatrix(glm::mat4(1.0f))
	{
		// Pipeline Creation.
		m_Pipeline = std::make_shared<Pipeline>(m_DescriptorSet, vertPath, fragPath);

		// Vertex Buffer creation.
		m_VBO = std::make_shared<VertexBuffer>(m_Vertices);
		m_VertexCount = static_cast<uint32_t>(m_Vertices.size());
		// Index Buffer creation. (If there is one passed.)
		if (m_Indices.size() != 0)
		{
			m_IBO = std::make_shared<IndexBuffer>(m_Indices);
		}

		int bindingIndex = 0;
		for (const auto& bindingSpecs : m_DescriptorSet->GetShaderLayout())
		{
			switch (bindingSpecs.Type)
			{
				case ShaderBindingType::UNIFORM_BUFFER:

					// God damn, this is a long operation. Might wanna change this.
					m_UniformBuffers.insert(std::pair<int, Ref<UniformBuffer>>(bindingIndex, std::make_shared<UniformBuffer>(m_DescriptorSet, FromUsageTypeToSize(bindingSpecs.UniformUsage), bindingIndex)));
					break;

				case ShaderBindingType::TEXTURE_SAMPLER:
					if (bindingSpecs.UniformUsage == UniformUsageType::NORMAL_SAMPLER)
					{
						VkSampler sampler = normalTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
						m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
					}
					else if(bindingSpecs.UniformUsage == UniformUsageType::DIFFUSE_SAMPLER)
					{
						VkSampler sampler = diffuseTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
						m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
					}
					else if (bindingSpecs.UniformUsage == UniformUsageType::ROUGHNESS_METALLIC_SAMPLER)
					{
						VkSampler sampler = roughnessMetallicTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
						m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
					}
					break;
			}
			bindingIndex++;
		}
	}

	Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture,
		const Ref<Texture>& normalTexture, const Ref<Texture>& roughnessTexture, const Ref<Texture>& metallicTexture, const Ref<Texture>& AOTexture, const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath)
	{
		// Pipeline Creation.
		m_Pipeline = std::make_shared<Pipeline>(m_DescriptorSet, vertPath, fragPath);

		// Vertex Buffer creation.
		m_VBO = std::make_shared<VertexBuffer>(m_Vertices);
		m_VertexCount = static_cast<uint32_t>(m_Vertices.size());
		// Index Buffer creation. (If there is one passed.)
		if (m_Indices.size() != 0)
		{
			m_IBO = std::make_shared<IndexBuffer>(m_Indices);
		}

		int bindingIndex = 0;
		for (const auto& bindingSpecs : m_DescriptorSet->GetShaderLayout())
		{
			switch (bindingSpecs.Type)
			{
			case ShaderBindingType::UNIFORM_BUFFER:

				// God damn, this is a long operation. Might wanna change this.
				m_UniformBuffers.insert(std::pair<int, Ref<UniformBuffer>>(bindingIndex, std::make_shared<UniformBuffer>(m_DescriptorSet, FromUsageTypeToSize(bindingSpecs.UniformUsage), bindingIndex)));
				break;

			case ShaderBindingType::TEXTURE_SAMPLER:
				if (bindingSpecs.UniformUsage == UniformUsageType::NORMAL_SAMPLER)
				{
					VkSampler sampler = normalTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
					m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
				}
				else if (bindingSpecs.UniformUsage == UniformUsageType::DIFFUSE_SAMPLER)
				{
					VkSampler sampler = diffuseTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
					m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
				}
				else if (bindingSpecs.UniformUsage == UniformUsageType::ROUGHNESS_SAMPLER)
				{
					VkSampler sampler = roughnessTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
					m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
				}
				else if (bindingSpecs.UniformUsage == UniformUsageType::AMBIENT_OCCLUSION_SAMPLER)
				{
					VkSampler sampler = AOTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
					m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
				}
				else if (bindingSpecs.UniformUsage == UniformUsageType::METALLIC_SAMPLER)
				{
					VkSampler sampler = metallicTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingIndex);
					m_ImageSamplers.insert(std::pair<int, VkSampler>(bindingIndex, sampler));
				}
				break;
			}
			bindingIndex++;
		}
	}

	Mesh::Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<DescriptorSet>& dscSet, const std::string& vertPath, const std::string& fragPath)
		:m_DescriptorSet(dscSet), m_ModelMatrix(glm::mat4(1.0f)), m_VertexCount(vertexCount)
	{
		// Pipeline Creation.
		m_Pipeline = std::make_shared<Pipeline>(m_DescriptorSet, vertPath, fragPath);

		// Vertex Buffer creation.
		m_VBO = std::make_shared<VertexBuffer>(vertices, vertexBufferSize);

		// Index Buffer creation. (If there is one passed.)
		if (m_Indices.size() != 0)
		{
			m_IBO = std::make_shared<IndexBuffer>(m_Indices);
		}

		int bindingIndex = 0;
		for (const auto& bindingSpecs : m_DescriptorSet->GetShaderLayout())
		{
			switch (bindingSpecs.Type)
			{
			case ShaderBindingType::UNIFORM_BUFFER:

				// God damn, this is a long operation. Might wanna change this.
				m_UniformBuffers.insert(std::pair<int, Ref<UniformBuffer>>(bindingIndex, std::make_shared<UniformBuffer>(m_DescriptorSet, FromUsageTypeToSize(bindingSpecs.UniformUsage), bindingIndex)));
				break;
			}
			bindingIndex++;
		}
	}

	Mesh::~Mesh()
	{
		for (const auto& sampler : m_ImageSamplers)
		{
			vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), sampler.second, nullptr);
		}
	}

	void Mesh::UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize)
	{
		const VkDeviceMemory& bufferMemoryToUpdate = FindBufferMemory(bufferIndex);
		void* bufferHandle;
		vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), bufferMemoryToUpdate, 0, dataSize, 0, &bufferHandle);
		memcpy(bufferHandle, dataToCopy, dataSize);
		vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), bufferMemoryToUpdate);
	}
	void Mesh::OnResize()
	{
		m_Pipeline->OnResize();
	}
	void Mesh::Rotate(const float& degree, const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::rotate(m_ModelMatrix, glm::radians(degree), glm::vec3(x, y, z));
		UpdateUniformBuffer(0, &m_ModelMatrix, sizeof(m_ModelMatrix));
	}
	void Mesh::Translate(const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::translate(m_ModelMatrix, glm::vec3(x, y, z));
		UpdateUniformBuffer(0, &m_ModelMatrix, sizeof(m_ModelMatrix));
	}
	void Mesh::Scale(const float& x, const float& y, const float& z)
	{
		m_ModelMatrix = glm::scale(m_ModelMatrix, glm::vec3(x, y, z));
		UpdateUniformBuffer(0, &m_ModelMatrix, sizeof(m_ModelMatrix));
	}
	const VkDeviceMemory& Mesh::FindBufferMemory(uint32_t bufferIndex)
	{
		// Search uniform buffers.
		for (auto& buffer : m_UniformBuffers)
		{
			if (buffer.first == bufferIndex)
			{
				return buffer.second->GetBufferMemory();
			}
		}
		return nullptr;
	}
}
