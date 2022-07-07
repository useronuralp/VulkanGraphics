#include "Texture.h"
#include <iostream>
namespace Anor
{
	Texture::Texture(const char* path)
	{
        int texWidth, texHeight, texChannels;
        m_Pixels = stbi_load(path, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                
        m_ImageSize = texWidth * texHeight * 4;

        if (!m_Pixels)
        {
            std::cerr << "Failed to load texture images!" << std::endl;
            __debugbreak();
        }

        //TO DO: Use the  function CreateBuffer from the Buffer class.

	}
}