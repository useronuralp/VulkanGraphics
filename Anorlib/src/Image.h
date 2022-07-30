#pragma once
#include "vulkan/vulkan.h"
#include "core.h"
#include <string>
namespace Anor
{
	class Image
	{
	public:
		Image(const char* imagePathToLoad, VkFormat imageFormat);
		Image(uint32_t width, uint32_t height, VkFormat imageFormat);

		const VkImage&			GetVKImage()		{ return m_Image; }
		const VkDeviceMemory&	GetVKImageMemory()	{ return m_ImageMemory; }
		const VkImageView&		GetImageView()		{ return m_ImageView; }
		~Image();

		uint32_t			GetHeight()		{ return m_Height; }
		uint32_t			GetWidth()		{ return m_Width; }
		VkDeviceSize		GetImageSize()	{ return m_ImageSize; }
		const std::string&	GetPath()		{ return m_Path; }
	private:
		void TransitionImageLayout(VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
		void CopyBufferToImage(const VkBuffer& buffer, uint32_t width, uint32_t height);
		void SetupImage(uint32_t width, uint32_t height, VkFormat imageFormat);
	private:
		VkImage			m_Image			= VK_NULL_HANDLE;
		VkDeviceMemory	m_ImageMemory	= VK_NULL_HANDLE;
		VkImageView		m_ImageView		= VK_NULL_HANDLE;

		VkDeviceSize m_ImageSize;
		std::string m_Path;
		uint32_t m_Height;
		uint32_t m_Width;
		uint32_t m_ChannelCount;
	};
}