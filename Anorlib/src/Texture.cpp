#include "Texture.h"
#include <iostream>
#include "core.h"
#include <stb_image.h>
namespace Anor
{
	Texture::Texture(const char* path)
        :m_Path(path)
	{
        int texWidth, texHeight, texChannels;
        m_Pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                
        m_ImageSize = texWidth * texHeight * 4;
        m_Width = texWidth;
        m_Channels = texChannels;
        m_Height = texHeight;

        ASSERT(m_Pixels, "Failed to load texture.");
	}
    void Texture::FreePixels()
    {
        stbi_image_free(m_Pixels);
    }
}