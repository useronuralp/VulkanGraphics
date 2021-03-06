#include "DescriptorSet.h"
#include "VulkanApplication.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include <iostream>
/*
* Descriptors can be changed in between draw calls. This way shaders can access different resources.
*/
namespace Anor
{
	DescriptorSet::DescriptorSet(const std::vector<ShaderBinding>& shaderBindings) // Descriptors are "pointers" to a resource. Programmer defines these resources.
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.resize(shaderBindings.size());
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.resize(shaderBindings.size());
		
		uint32_t bindingIndex = 0;
		for (const auto& primitive : shaderBindings)
		{
			switch (primitive)
			{
				case ShaderBinding::UNIFORM_BUFFER:

					// For the Uniform Buffer object.
					bindings[bindingIndex].binding = bindingIndex; // binding number used in the shader.
					bindings[bindingIndex].descriptorCount = 1;
					bindings[bindingIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					bindings[bindingIndex].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
					bindings[bindingIndex].pImmutableSamplers = nullptr;

					poolSizes[bindingIndex].descriptorCount = 1;
					poolSizes[bindingIndex].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
					break;
				case ShaderBinding::IMAGE_SAMPLER:
					// For the texture sampler in the fragment shader 
					bindings[bindingIndex].binding = bindingIndex;
					bindings[bindingIndex].descriptorCount = 1;
					bindings[bindingIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					bindings[bindingIndex].pImmutableSamplers = nullptr;
					bindings[bindingIndex].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		
					poolSizes[bindingIndex].descriptorCount = 1;
					poolSizes[bindingIndex].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
					break;

			}
			bindingIndex++;
		}

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		ASSERT(vkCreateDescriptorSetLayout(VulkanApplication::s_Device->GetVKDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) == VK_SUCCESS, "Failed to create descriptor set layout!");


		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		ASSERT(vkCreateDescriptorPool(VulkanApplication::s_Device->GetVKDevice(), &poolInfo, nullptr, &m_DescriptorPool) == VK_SUCCESS, "Failed to create descriptor pool!");

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_DescriptorSetLayout;

		ASSERT(vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet) == VK_SUCCESS, "Failed to allocate descriptor sets!");
	}
	DescriptorSet::~DescriptorSet()
	{
		vkDestroyDescriptorPool(VulkanApplication::s_Device->GetVKDevice(), m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(VulkanApplication::s_Device->GetVKDevice(), m_DescriptorSetLayout, nullptr);
	}
}