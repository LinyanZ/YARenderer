#pragma once

#include "pch.h"
#include "Timer.h"

#include "event/Event.h"

struct WindowProps
{
    std::wstring Title;
    unsigned int Width;
    unsigned int Height;

    WindowProps(const std::wstring &title = L"PBR Renderer",
                unsigned int width = 1920,
                unsigned int height = 900)
        : Title(title), Width(width), Height(height) {}
};

class Window
{
public:
    Window(const WindowProps &props = WindowProps());
    ~Window();

    void OnEvent(Event &e) { Callback(e); }
    void OnUpdate(Timer &timer);
    void OnResize(unsigned int width, unsigned int height);

    HWND GetHandle() { return m_HMainWnd; }
    unsigned int GetWidth() { return m_Width; }
    unsigned int GetHeight() { return m_Height; }
    float AspectRatio() { return static_cast<float>(m_Width) / m_Height; }

    using EventCallbackFn = std::function<void(Event &)>;
    EventCallbackFn Callback = nullptr;

private:
    void CalculateFrameStats(Timer &timer);

    HWND m_HMainWnd = 0;
    WNDCLASSEXW m_WndClass;

    std::wstring m_Title;
    unsigned int m_Width;
    unsigned int m_Height;
};
