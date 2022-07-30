#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "core.h"
#include "Image.h"
namespace Anor
{
	class DescriptorSet;
	class Texture
	{
	public:
		Texture() = default;
		Texture(const char* path, VkFormat imageFormat);
		const std::string& GetPath() { return m_Image->GetPath(); }
		VkDeviceSize GetImageSize() { return m_Image->GetImageSize(); }
		uint32_t GetWidth() { return m_Image->GetWidth(); }
		uint32_t GetHeight() { return m_Image->GetHeight(); }

		VkSampler CreateSamplerFromThisTexture(const Ref<DescriptorSet>& dscSet, uint32_t bindingIndex);
	private:
		Ref<Image> m_Image;
	};
}