#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <string>
#include <vector>
namespace Anor
{
	enum class ImageType
	{
		COLOR,
		DEPTH
	};
	class Image
	{
	public:
		Image(std::vector<std::string> textures, VkFormat imageFormat);
		Image(uint32_t width, uint32_t height, VkFormat imageFormat, ImageType imageType = ImageType::COLOR);

		const VkImage&			GetVKImage()		{ return m_Image; }
		const VkDeviceMemory&	GetVKImageMemory()	{ return m_ImageMemory; }
		const VkImageView&		GetImageView()		{ return m_ImageView; }
		~Image();

		uint32_t			GetHeight()		{ return m_Height; }
		uint32_t			GetWidth()		{ return m_Width; }
		VkDeviceSize		GetImageSize()	{ return m_ImageSize; }
		const std::string&	GetPath()		{ return m_Path; }
		uint32_t			GetMipLevel()   { return m_MipLevels; }
	private:
		void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height);
		void SetupImage(uint32_t width, uint32_t height, VkFormat imageFormat, ImageType imageType = ImageType::COLOR);
		void GenerateMipmaps();
	private:
		VkImage			m_Image			= VK_NULL_HANDLE;
		VkDeviceMemory	m_ImageMemory	= VK_NULL_HANDLE;
		VkImageView		m_ImageView		= VK_NULL_HANDLE;

		VkFormat		m_ImageFormat;

		uint32_t		m_MipLevels = 1;
		bool			m_IsCubemap = false;

		VkDeviceSize m_ImageSize;
		VkDeviceSize m_LayerSize;
		std::string m_Path;
		std::vector<std::string> m_CubemapTexPaths;

		uint32_t m_Height;
		uint32_t m_Width;
		uint32_t m_ChannelCount;
	};
}