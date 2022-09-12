#pragma once
#include "vulkan/vulkan.h"
#include <vector>
#include "glm/glm.hpp"
#include <vector>
#include "core.h"
namespace Anor
{
	class Swapchain;
	class DescriptorSet;
	class RenderPass;
	class Pipeline
	{
	public:
		struct Specs
		{
			Ref<RenderPass>					    RenderPass;
			VkDescriptorSetLayout			    DescriptorBindingSpecs; 
			std::string						    VertexShaderPath; 
			std::string						    FragmentShaderPath; 
			VkPolygonMode					    PolygonMode;
			VkCullModeFlags					    CullMode; 
			VkFrontFace						    FrontFace; 
			VkBool32						    EnableDepthBias;
			float							    DepthBiasConstantFactor;
			float							    DepthBiasClamp;
			float							    DepthBiasSlopeFactor;
			VkBool32						    EnableDepthTesting;
			VkBool32						    EnableDepthWriting;
			VkCompareOp						    DepthCompareOp;
			uint32_t						    ViewportWidth  = UINT32_MAX;
			uint32_t						    ViewportHeight = UINT32_MAX;
			std::vector<VkVertexInputBindingDescription> pVertexBindingDesc;
			VkPrimitiveTopology				    PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkPipelineColorBlendAttachmentState ColorBlendAttachmentState;
			std::vector<VkVertexInputAttributeDescription>  pVertexAttributeDescriptons;
		};
	public:
		Pipeline(const Specs& CI);
		~Pipeline();
		void OnResize();
		const VkPipeline&		GetVKPipeline()		{ return m_Pipeline;		}
		const VkPipelineLayout& GetPipelineLayout()	{ return m_PipelineLayout;	}
	private:
		void Init();
		void Cleanup();
		VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);
	private:
		VkPipeline					m_Pipeline;
		VkPipelineLayout			m_PipelineLayout;
		std::vector<VkDynamicState> m_DynamicStates;
		VkDescriptorSetLayout		m_DescriptorSetLayout;

		// Used to store the configuration of the pipeline. When screen is resized, these values are reused that is why we are storing them here.
		Specs	m_CI;
	};
}