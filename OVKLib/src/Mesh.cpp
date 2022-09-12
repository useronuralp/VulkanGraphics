#include "Mesh.h"
#include "DescriptorSet.h"
#include "Pipeline.h"
#include "CommandBuffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Texture.h"
#include "RenderPass.h"

namespace Anor
{
	void Mesh::DrawIndexed(const VkCommandBuffer& cmdBuffer)
	{
		CommandBuffer::BindPipeline(cmdBuffer, m_ActiveConfiguration.Pipeline);
		CommandBuffer::BindDescriptorSet(cmdBuffer, m_ActiveConfiguration.Pipeline->GetPipelineLayout(), m_ActiveConfiguration.DescriptorSet);
		CommandBuffer::BindVertexBuffer(cmdBuffer, m_VBO->GetVKBuffer(), 0);
		CommandBuffer::BindIndexBuffer(cmdBuffer, m_IBO->GetVKBuffer());
		CommandBuffer::DrawIndexed(cmdBuffer, m_IndexCount);
	}
	void Mesh::Draw(const VkCommandBuffer& cmdBuffer)
	{
		CommandBuffer::BindPipeline(cmdBuffer, m_ActiveConfiguration.Pipeline);
		CommandBuffer::BindDescriptorSet(cmdBuffer, m_ActiveConfiguration.Pipeline->GetPipelineLayout(), m_ActiveConfiguration.DescriptorSet);
		CommandBuffer::BindVertexBuffer(cmdBuffer, m_VBO->GetVKBuffer(), 0);
		CommandBuffer::Draw(cmdBuffer, m_VertexCount);
	}
	Mesh::Mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture,
		const Ref<Texture>& roughnessMetallicTexture, const Ref<Texture>& shadowMap)
		:m_ModelMatrix(glm::mat4(1.0f)), m_Albedo(diffuseTexture), m_Normals(normalTexture), m_RoughnessMetallic(roughnessMetallicTexture)
	{
		// Vertex Buffer creation.
		m_VBO = std::make_shared<VertexBuffer>(vertices);
		m_VertexCount = static_cast<uint32_t>(vertices.size());
		m_IndexCount = indices.size();
		// Index Buffer creation. (If there is one passed.)
		if (indices.size() != 0)
		{
			m_IBO = std::make_shared<IndexBuffer>(indices);
		}
		m_ShadowMap = shadowMap;
	}

	Mesh::Mesh(const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture, const Ref<Texture>& roughnessMetallicTexture, const Ref<Texture>& shadowMap)
		:m_ModelMatrix(glm::mat4(1.0f)), m_Albedo(diffuseTexture), m_Normals(normalTexture), m_RoughnessMetallic(roughnessMetallicTexture)
	{
		m_ShadowMap = shadowMap;
	}

	Mesh::Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<CubemapTexture>& cubemapTex)
		:m_ModelMatrix(glm::mat4(1.0f)), m_VertexCount(vertexCount), m_CubemapTexture(cubemapTex)
	{
		// Vertex Buffer creation.
		m_VBO = std::make_shared<VertexBuffer>(vertices, vertexBufferSize);
		m_VertexCount = vertexCount;
	}

	Mesh::Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const std::vector<uint32_t>& indices)
	{
		m_VBO = std::make_shared<VertexBuffer>(vertices, vertexBufferSize);
		m_VertexCount = vertexCount;
		m_IndexCount = indices.size();
		if (indices.size() != 0)
		{
			m_IBO = std::make_shared<IndexBuffer>(indices);
		}
	}

	Mesh::~Mesh()
	{
		for (const auto& config : m_Configurations)
		{
			for (const auto& sampler : config.ImageSamplers)
			{
				vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), sampler, nullptr);
			}
		}
	}

	void Mesh::AddConfiguration(const char* configName, Pipeline::Specs pipelineCI, std::vector<DescriptorBindingSpecs> descriptorLayout)
	{
		// Create a descriptor set PER MESH.
		Ref<DescriptorSet> newDescriptorSet = std::make_shared<DescriptorSet>(descriptorLayout);

		// Set the layout of the pipeline to the newly created descriptor set.
		pipelineCI.DescriptorBindingSpecs = newDescriptorSet->GetVKDescriptorSetLayout();

		// Create the pipeline.
		Ref<Pipeline> newPipeline = std::make_shared<Pipeline>(pipelineCI);

		Configuration newConfiguration{};
		newConfiguration.ConfigurationName = configName;
		newConfiguration.Pipeline = newPipeline;
		newConfiguration.DescriptorSet = newDescriptorSet;

		int bindingIndex = 0;
		for (const auto& bindingSpecs : newConfiguration.DescriptorSet->GetLayout())
		{
			if (bindingSpecs.Type == Type::UNIFORM_BUFFER)
			{
				Ref<UniformBuffer> UB = std::make_shared<UniformBuffer>(newConfiguration.DescriptorSet, bindingSpecs.Size * bindingSpecs.Count, bindingIndex);
				newConfiguration.UniformBuffers.insert(std::make_pair(bindingIndex, UB));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_NORMAL)
			{
				newConfiguration.ImageSamplers.push_back(m_Normals->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_DIFFUSE)
			{
				newConfiguration.ImageSamplers.push_back(m_Albedo->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC)
			{
				newConfiguration.ImageSamplers.push_back(m_RoughnessMetallic->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_CUBEMAP)
			{
				newConfiguration.ImageSamplers.push_back(m_CubemapTexture->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_SHADOWMAP)
			{
				newConfiguration.ImageSamplers.push_back(m_ShadowMap->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex, ImageType::DEPTH));
			}
			bindingIndex++;
		}
		m_Configurations.push_back(newConfiguration);
	}

	void Mesh::AddConfiguration(const char* configName, Ref<Pipeline> pipeline, std::vector<DescriptorBindingSpecs> descriptorLayout)
	{
		// Create a descriptor set PER MESH.
		Ref<DescriptorSet> newDescriptorSet = std::make_shared<DescriptorSet>(descriptorLayout);

		// Create the pipeline.
		Ref<Pipeline> newPipeline = pipeline;

		Configuration newConfiguration{};
		newConfiguration.ConfigurationName = configName;
		newConfiguration.Pipeline = newPipeline;
		newConfiguration.DescriptorSet = newDescriptorSet;

		int bindingIndex = 0;
		for (const auto& bindingSpecs : newConfiguration.DescriptorSet->GetLayout())
		{
			if (bindingSpecs.Type == Type::UNIFORM_BUFFER)
			{
				Ref<UniformBuffer> UB = std::make_shared<UniformBuffer>(newConfiguration.DescriptorSet, bindingSpecs.Size * bindingSpecs.Count, bindingIndex);
				newConfiguration.UniformBuffers.insert(std::make_pair(bindingIndex, UB));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_NORMAL)
			{
				newConfiguration.ImageSamplers.push_back(m_Normals->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_DIFFUSE)
			{
				newConfiguration.ImageSamplers.push_back(m_Albedo->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC)
			{
				newConfiguration.ImageSamplers.push_back(m_RoughnessMetallic->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_CUBEMAP)
			{
				newConfiguration.ImageSamplers.push_back(m_CubemapTexture->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_SHADOWMAP)
			{
				newConfiguration.ImageSamplers.push_back(m_ShadowMap->CreateSamplerFromThisTexture(newConfiguration.DescriptorSet, bindingIndex, ImageType::DEPTH));
			}
			bindingIndex++;
		}
		m_Configurations.push_back(newConfiguration);
	}

	void Mesh::SetActiveConfiguration(const char* configName)
	{
		for (const auto& config : m_Configurations)
		{
			if (config.ConfigurationName == configName)
			{
				m_ActiveConfiguration = config;
			}
		}
	}

	void Mesh::OnResize()
	{
		m_ActiveConfiguration.Pipeline->OnResize();
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
	void Mesh::UpdateUniformBuffer(uint32_t bufferIndex, void* dataToCopy, size_t dataSize)
	{
		const VkDeviceMemory& bufferMemoryToUpdate = FindBufferMemory(bufferIndex);
		void* bufferHandle;
		vkMapMemory(VulkanApplication::s_Device->GetVKDevice(), bufferMemoryToUpdate, 0, dataSize, 0, &bufferHandle);
		memcpy(bufferHandle, dataToCopy, dataSize);
		vkUnmapMemory(VulkanApplication::s_Device->GetVKDevice(), bufferMemoryToUpdate);
	}
	const VkDeviceMemory& Mesh::FindBufferMemory(uint32_t bufferIndex)
	{
		// Search uniform buffers.
		for (auto& buffer : m_ActiveConfiguration.UniformBuffers)
		{
			if (buffer.first == bufferIndex)
			{
				return buffer.second->GetBufferMemory();
			}
		}
		return nullptr;
	}
}
