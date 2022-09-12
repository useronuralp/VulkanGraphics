#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include "core.h"
#include "Image.h"
#include <array>
namespace OVK
{
	class DescriptorSet;
	class Texture
	{
	public:
		Texture() = default;
		Texture(const char* path, VkFormat imageFormat);
		Texture(uint32_t width, uint32_t height, VkFormat imageFormat, ImageType imageType = ImageType::COLOR);
		const std::string& GetPath() { return m_Image->GetPath(); }
		VkDeviceSize GetImageSize() { return m_Image->GetImageSize(); }
		uint32_t GetWidth() { return m_Image->GetWidth(); }
		uint32_t GetHeight() { return m_Image->GetHeight(); }
		Ref<Image> GetImage() { return m_Image; }

		VkSampler CreateSamplerFromThisTexture(const Ref<DescriptorSet>& dscSet, uint32_t bindingIndex, ImageType imageType = ImageType::COLOR);
	private:
		Ref<Image> m_Image;
	};
	class CubemapTexture
	{
	public:
		CubemapTexture(std::array<std::string, 6> texturePaths, VkFormat imageFormat);
		VkSampler CreateSamplerFromThisTexture(const Ref<DescriptorSet>& dscSet, uint32_t bindingIndex);
	private:
		Ref<Image> m_Image;
	};
}