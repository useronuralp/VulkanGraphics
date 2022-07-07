#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
namespace Anor
{
	class RenderPass;
	class Swapchain;
	class LogicalDevice;
	class DescriptorSet;
	class Pipeline
	{
	public:
		struct CreateInfo
		{
			LogicalDevice* pLogicalDevice;
			Swapchain* pSwapchain;
			RenderPass* pRenderPass;
			DescriptorSet* pDescriptorSet;
		};

	public:
		Pipeline(CreateInfo& createInfo);
		~Pipeline();
		VkPipeline GetVKPipeline() { return m_Pipeline; }
		VkPipelineLayout GetPipelineLayout() { return m_PipelineLayout; }
		VkDescriptorSetLayout GetDescriptorSetLayout() { return m_DescriptorSetLayout; }
	private:
		VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);
	private:
		VkPipeline					m_Pipeline;
		VkPipelineLayout			m_PipelineLayout;
		std::vector<VkDynamicState> m_DynamicStates;
		VkDescriptorSetLayout		m_DescriptorSetLayout;
		// !!!!!!!!!WARNING!!!!!!!!!!: WATCH OUT FOR DANGLING POINTERS HERE. THE FOLLOWING POINTERS SHOULD BE SET TO NULL WHEN THE POINTED MEMORY IS FREED.
		LogicalDevice*				m_Device;
		Swapchain*					m_Swapchain;
		RenderPass*					m_RenderPass;
		DescriptorSet*				m_DescriptorSet;
	};
}