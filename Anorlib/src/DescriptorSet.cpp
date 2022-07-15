#include "DescriptorSet.h"
#include "LogicalDevice.h"
#include "Pipeline.h"
#include <iostream>
/*
* Descriptors can be changed in between draw calls. This way shaders can access different resources.
*/
namespace Anor
{
	DescriptorSet::DescriptorSet(const Ref<LogicalDevice>& device) // Descriptors are "pointers" to a resource. Programmer defines these resources.
		:m_Device(device)
	{
		// TO DO: Think about exposing this part to the user.
		VkDescriptorSetLayoutBinding uboLayoutBinding{};
		uboLayoutBinding.binding = 0; // binding number used in the shader.
		uboLayoutBinding.descriptorCount = 1;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uboLayoutBinding.pImmutableSamplers = nullptr;

		// For the texture sampler in the fragment shader 
		VkDescriptorSetLayoutBinding samplerLayoutBinding{};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings =
		{ 
			uboLayoutBinding,
			samplerLayoutBinding
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		ASSERT(vkCreateDescriptorSetLayout(m_Device->GetVKDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) == VK_SUCCESS, "Failed to create descriptor set layout!");

		std::array<VkDescriptorPoolSize, 2> poolSizes{};
		// Descriptor pool creation. O: Uniform buffer, 1: Sampler
		poolSizes[0].descriptorCount = 1;
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[1].descriptorCount = 1;
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		ASSERT(vkCreateDescriptorPool(m_Device->GetVKDevice(), &poolInfo, nullptr, &m_DescriptorPool) == VK_SUCCESS, "Failed to create descriptor pool!");

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_DescriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_DescriptorSetLayout;

		ASSERT(vkAllocateDescriptorSets(m_Device->GetVKDevice(), &allocInfo, &m_DescriptorSet) == VK_SUCCESS, "Failed to allocate descriptor sets!");
	}
	DescriptorSet::~DescriptorSet()
	{
		vkDestroyDescriptorPool(m_Device->GetVKDevice(), m_DescriptorPool, nullptr);
		vkDestroyDescriptorSetLayout(m_Device->GetVKDevice(), m_DescriptorSetLayout, nullptr);
	}
}