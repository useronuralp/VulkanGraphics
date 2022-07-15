#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
#include "core.h"
namespace Anor
{
	class RenderPass;
	class Swapchain;
	class LogicalDevice;
	class DescriptorSet;
	class Pipeline
	{
	public:
		Pipeline(const Ref<LogicalDevice>& device, const Ref<Swapchain>& swapchain, const Ref<RenderPass>& renderPass, const Ref<DescriptorSet>& dscSet);
		~Pipeline();
		const VkPipeline& GetVKPipeline()						{ return m_Pipeline; }
		const VkPipelineLayout& GetPipelineLayout()				{ return m_PipelineLayout; }
	private:
		VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);
	private:
		VkPipeline					m_Pipeline;
		VkPipelineLayout			m_PipelineLayout;
		std::vector<VkDynamicState> m_DynamicStates;

		Ref<LogicalDevice>				m_Device;
		Ref<Swapchain>					m_Swapchain;
		Ref<RenderPass>					m_RenderPass;
		Ref<DescriptorSet>				m_DescriptorSet;
	};
}