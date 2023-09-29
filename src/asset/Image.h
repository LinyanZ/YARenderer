#pragma once

#include "pch.h"

class Image
{
public:
    static std::shared_ptr<Image> FromFile(const std::string &filename, int channels = 4);

    int Width() const { return m_Width; }
    int Height() const { return m_Height; }
    int Channels() const { return m_Channels; }
    int BytesPerPixel() const { return m_Channels * (m_HDR ? sizeof(float) : sizeof(unsigned char)); }
    int Pitch() const { return m_Width * BytesPerPixel(); }
    int ByteSize() const { return m_Width * m_Height * BytesPerPixel(); }
    bool IsHDR() const { return m_HDR; }

    template <typename T>
    const T *Pixels() const { return reinterpret_cast<const T *>(m_Pixels.get()); }

private:
    Image() {}

    int m_Width = 0;
    int m_Height = 0;
    int m_Channels = 0;
    bool m_HDR = false;
    std::unique_ptr<unsigned char> m_Pixels = nullptr;
};
