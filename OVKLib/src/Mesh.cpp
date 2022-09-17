#include "Mesh.h"
#include "Model.h"
#include "DescriptorSet.h"
#include "CommandBuffer.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Texture.h"
#include "Buffer.h"
namespace OVK
{

	Mesh::Mesh(const std::vector<float>& vertices, const std::vector<uint32_t>& indices, const Ref<Texture>& diffuseTexture, const Ref<Texture>& normalTexture,
		const Ref<Texture>& roughnessMetallicTexture, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout, const Ref<Texture>& shadowMap)
		: m_Albedo(diffuseTexture), m_Normals(normalTexture), m_RoughnessMetallic(roughnessMetallicTexture), m_ShadowMap(shadowMap)
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool->GetDescriptorPool();
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout->GetDescriptorLayout();

		VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet);
		ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");


		for (const auto& bindingSpecs : layout->GetBindingSpecs())
		{
			if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_NORMAL)
			{
				m_Samplers.push_back(m_Normals->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_DIFFUSE)
			{
				m_Samplers.push_back(m_Albedo->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding));
			}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_ROUGHNESSMETALLIC)
			{
				m_Samplers.push_back(m_RoughnessMetallic->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding));
			}
			//else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_CUBEMAP)
			//{
			//	m_Samplers.push_back(m_CubemapTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding));
			//}
			else if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_SHADOWMAP)
			{
				m_Samplers.push_back(m_ShadowMap->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding, ImageType::DEPTH));
			}
		}

		// Vertex Buffer creation.
		m_VBO = std::make_unique<VertexBuffer>(vertices);
		m_VertexCount = static_cast<uint32_t>(vertices.size());
		m_IndexCount = indices.size();
		// Index Buffer creation. (If there is one passed.)
		if (indices.size() != 0)
		{
			m_IBO = std::make_unique<IndexBuffer>(indices);
		}
		m_ShadowMap = shadowMap;
	}

	Mesh::Mesh(const float* vertices, size_t vertexBufferSize, uint32_t vertexCount, const Ref<CubemapTexture>& cubemapTex, Ref<DescriptorPool> pool, Ref<DescriptorLayout> layout)
		: m_VertexCount(vertexCount), m_CubemapTexture(cubemapTex)
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = pool->GetDescriptorPool();
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &layout->GetDescriptorLayout();

		VkResult rslt = vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet);
		ASSERT(rslt == VK_SUCCESS, "Failed to allocate descriptor sets!");

		for (const auto& bindingSpecs : layout->GetBindingSpecs())
		{
			if (bindingSpecs.Type == Type::TEXTURE_SAMPLER_CUBEMAP)
			{
				m_Samplers.push_back(m_CubemapTexture->CreateSamplerFromThisTexture(m_DescriptorSet, bindingSpecs.Binding));
			}
		}

		// Vertex Buffer creation.
		m_VBO = std::make_unique<VertexBuffer>(vertices, vertexBufferSize);
		m_VertexCount = vertexCount;
	}

	Mesh::~Mesh()
	{
		for (const auto& sampler : m_Samplers)
		{
			vkDestroySampler(VulkanApplication::s_Device->GetVKDevice(), sampler, nullptr);
		}
	}

	void Mesh::WriteDescriptorSet(const VkWriteDescriptorSet& descriptorWrite)
	{
		vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
	}
}
