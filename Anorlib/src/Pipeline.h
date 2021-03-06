#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <array>
#include "core.h"
namespace Anor
{
	class Swapchain;
	class DescriptorSet;
	class Pipeline
	{
	public:
		Pipeline(const Ref<DescriptorSet>& dscSet);
		~Pipeline();
		void OnResize();
		const VkPipeline& GetVKPipeline()						{ return m_Pipeline; }
		const VkPipelineLayout& GetPipelineLayout()				{ return m_PipelineLayout; }
	private:
		void Init(const Ref<DescriptorSet>& dscSet);
		void Cleanup();
		VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);
	private:
		VkPipeline					m_Pipeline;
		VkPipelineLayout			m_PipelineLayout;
		std::vector<VkDynamicState> m_DynamicStates;

		Ref<DescriptorSet>				m_DescriptorSet;
	};
}