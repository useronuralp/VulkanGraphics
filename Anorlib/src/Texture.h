#pragma once
#include <stb_image.h>
#include <vulkan/vulkan.h>
namespace Anor
{
	class Texture
	{
	public:
		Texture(const char* path);
	private:
		stbi_uc* m_Pixels;
		VkDeviceSize m_ImageSize;
		VkBuffer m_StagingBuffer;
		VkDeviceMemory m_StagingBufferMemory;
	};
}