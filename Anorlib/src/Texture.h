#pragma once
#include <vulkan/vulkan.h>
#include <string>
namespace Anor
{
	class Texture
	{
		typedef unsigned char stbi_uc;
	public:
		Texture() = default;
		Texture(const char* path);
		const std::string& GetPath() { return m_Path; }
		stbi_uc* GetPixels() { return m_Pixels; }
		VkDeviceSize GetImageSize() { return m_ImageSize; }
		uint32_t GetWidth() { return m_Width; }
		void FreePixels();
		uint32_t GetHeight() { return m_Height; }
		uint32_t GetChannels() { return m_Channels; }
	private:
		stbi_uc*	 m_Pixels;
		VkDeviceSize m_ImageSize;
		std::string  m_Path;

		uint32_t m_Width;
		uint32_t m_Height;
		uint32_t m_Channels;
	};
}