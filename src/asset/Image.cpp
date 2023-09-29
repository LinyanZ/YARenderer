#include "pch.h"
#include "Image.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

std::shared_ptr<Image> Image::FromFile(const std::string &filename, int channels)
{
    LOG_INFO("Loading image: {}", filename);

    std::shared_ptr<Image> image(new Image());

    if (stbi_is_hdr(filename.c_str()))
    {
        float *pixels = stbi_loadf(filename.c_str(), &image->m_Width, &image->m_Height, &image->m_Channels, channels);
        if (pixels)
        {
            image->m_Pixels.reset(reinterpret_cast<unsigned char *>(pixels));
            image->m_HDR = true;
        }
    }
    else
    {
        unsigned char *pixels = stbi_load(filename.c_str(), &image->m_Width, &image->m_Height, &image->m_Channels, channels);
        if (pixels)
        {
            image->m_Pixels.reset(pixels);
            image->m_HDR = false;
        }
    }

    ASSERT(image->m_Pixels, "Failed to load image file: " + filename);

    if (channels > 0)
        image->m_Channels = channels;
    return image;
}