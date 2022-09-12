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
	DescriptorSet::DescriptorSet(const std::vector<DescriptorBindingSpecs>& layout) // Descriptors are "pointers" to a resource. Programmer defines these resources.
		:m_SetLayout(layout)
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;
		bindings.resize(m_SetLayout.size());
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.resize(m_SetLayout.size());
		
		uint32_t bindingIndex = 0;
		for (const auto& bindingSpecs : m_SetLayout)
		{
			if(bindingSpecs.Type == Type::UNIFORM_BUFFER)
			{
				// For the Uniform Buffer object.
				bindings[bindingIndex].binding = bindingSpecs.Binding; // binding number used in the shader.
				bindings[bindingIndex].descriptorCount = 1;
				bindings[bindingIndex].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				bindings[bindingIndex].stageFlags = FromShaderStageToDescriptorType(bindingSpecs.ShaderStage);
				bindings[bindingIndex].pImmutableSamplers = nullptr;

				poolSizes[bindingIndex].descriptorCount = 1;
				poolSizes[bindingIndex].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			}
			else
			{
				// For the texture sampler in the fragment shader 
				bindings[bindingIndex].binding = bindingSpecs.Binding;
				bindings[bindingIndex].descriptorCount = 1;
				bindings[bindingIndex].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				bindings[bindingIndex].pImmutableSamplers = nullptr;
				bindings[bindingIndex].stageFlags = FromShaderStageToDescriptorType(bindingSpecs.ShaderStage);
		
				poolSizes[bindingIndex].descriptorCount = 1;
				poolSizes[bindingIndex].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			}
			bindingIndex++;
		}

		// REMARK: This class currently allocates a SINGLE descriptor set whenever it is constructed. However, in reality, you can allocate more than one descriptor sets 
		// AT THE SAME TIME when you are calling the "vkAllocateDescriptorSets" command. You just need to create a vector of "VkDescriptorSetLayoutCreateInfo" instead of creating a single one.


		// A single "VkDescriptorSetLayoutCreateInfo" is created here.
		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		ASSERT(vkCreateDescriptorSetLayout(VulkanApplication::s_Device->GetVKDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) == VK_SUCCESS, "Failed to create descriptor set layout!");

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1; // Needs to reflect the number of descriptor sets you want to create. We are currently creating only 1.

		ASSERT(vkCreateDescriptorPool(VulkanApplication::s_Device->GetVKDevice(), &poolInfo, nullptr, &m_DescriptorPool) == VK_SUCCESS, "Failed to create descriptor pool!");


		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = 1; // Need to reflect the number of descriptor sets you are creating.
		allocInfo.pSetLayouts = &m_DescriptorSetLayout; 

		ASSERT(vkAllocateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet) == VK_SUCCESS, "Failed to allocate descriptor sets!");
	}
	void DescriptorSet::WriteDescriptorSet(uint32_t bindingIndex, const VkDescriptorBufferInfo& bufferInfo, const VkDescriptorImageInfo& imageInfo)
	{
		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = m_DescriptorSet;
		descriptorWrite.dstBinding = bindingIndex;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;
		descriptorWrite.pImageInfo = &imageInfo; // Optional
		descriptorWrite.pTexelBufferView = nullptr; // Optional
		vkUpdateDescriptorSets(VulkanApplication::s_Device->GetVKDevice(), 1, &descriptorWrite, 0, nullptr);
	}
	DescriptorSet::~DescriptorSet()
	{
		vkDestroyDescriptorPool(VulkanApplication::s_Device->GetVKDevice(), m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(VulkanApplication::s_Device->GetVKDevice(), m_DescriptorSetLayout, nullptr);
	}
}