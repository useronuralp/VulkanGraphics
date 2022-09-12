#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <vector>
namespace OVK
{
	enum class AttachmentType
	{
		DEPTH_STENCIL,
		COLOR,
	};
	class Attachment
	{
	public:
		struct CreateInfo
		{
			AttachmentType		  Type;
			uint32_t			  Index;
			VkFormat			  Format;
			VkSampleCountFlagBits SampleCount;
			VkAttachmentLoadOp	  LoadOp;
			VkAttachmentStoreOp	  StoreOp;
			VkAttachmentLoadOp	  StencilLoadOp;
			VkAttachmentStoreOp	  StencilStoreOp;
			VkImageLayout		  InitialLayout;
			VkImageLayout		  FinalLayout;
		};
	public:
		Attachment(CreateInfo CI);
	public:
		VkAttachmentDescription GetAttachmentDescription()	{ return m_Desc; }
		VkAttachmentReference	GetAttachmentReference()	{ return m_Ref; }
	private:
		VkAttachmentDescription m_Desc;
		VkAttachmentReference	m_Ref;
	};

	class RenderPass
	{
	public:
		const VkRenderPass& GetRenderPass() { return m_RenderPass; }
		RenderPass(const VkRenderPass renderPass) : m_RenderPass(renderPass) {}
		RenderPass(std::vector<Attachment>& colorAttachments, std::vector<Attachment>& depthAttachments);
		~RenderPass();
	private:
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	};
	static VkImageLayout FromAnorLayoutTypeToVulkan(AttachmentType type)
	{
		switch (type)
		{
			case AttachmentType::DEPTH_STENCIL: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			case AttachmentType::COLOR:			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
	}
}