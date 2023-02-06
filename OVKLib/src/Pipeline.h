#pragma once
#include "core.h"
// External
#include <vector>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan.h>
namespace OVK
{
	class DescriptorSetLayout;
	class Pipeline
	{
	public:
		struct Specs
		{
			// TO DO: Check these references here. Can you make them Unique<> ?
			VkRenderPass*						 pRenderPass;
			Ref<DescriptorSetLayout>				 DescriptorSetLayout; 
			std::string							 VertexShaderPath = "None";
			std::string							 FragmentShaderPath = "None";
			std::string							 GeometryShaderPath = "None";
			VkPolygonMode						 PolygonMode;
			VkCullModeFlags						 CullMode; 
			VkFrontFace							 FrontFace; 
			VkBool32							 EnableDepthBias;
			float								 DepthBiasConstantFactor;
			float								 DepthBiasClamp;
			float								 DepthBiasSlopeFactor;
			VkBool32							 EnableDepthTesting;
			VkBool32							 EnableDepthWriting;
			VkCompareOp							 DepthCompareOp;
			uint32_t							 ViewportWidth  = UINT32_MAX;
			uint32_t							 ViewportHeight = UINT32_MAX;
			std::vector<VkPushConstantRange>	 PushConstantRanges;
			VkPrimitiveTopology					 PrimitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			VkPipelineColorBlendAttachmentState	 ColorBlendAttachmentState;
			VkVertexInputBindingDescription*	 pVertexInputBindingDescriptions = nullptr;
			uint32_t							 VertexInputBindingCount = 0;
			uint32_t							 VertexInputAttributeCount = 0;
			VkVertexInputAttributeDescription*	 pVertexInputAttributeDescriptons = nullptr;
		};
	public:
		Pipeline(const Specs& CI);
		~Pipeline();
		void ReConstruct();
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

		// Used to store the configuration of the pipeline. When screen is resized, these values are reused that is why we are storing them here.
		Specs	m_CI;
	};
}