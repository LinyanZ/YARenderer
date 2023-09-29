#include "Window.h"
#include "Application.h"
#include "backends/imgui_impl_win32.h"

#include "event/ApplicationEvent.h"
#include "event/MouseEvent.h"
#include "event/KeyEvent.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window(const WindowProps &props)
{
    m_Title = props.Title;

    // register the window class

    m_WndClass = {sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, m_Title.c_str(), nullptr};
    auto result = ::RegisterClassExW(&m_WndClass);
    ASSERT(result, "RegisterClass FAILED.");

    // center the window within the screen, clamp to (0, 0) (top-left corner)

    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    int windowX = std::max<int>(0, (screenWidth - props.Width) / 2);
    int windowY = std::max<int>(0, (screenHeight - props.Height) / 2);

    // create the window instance

    m_HMainWnd = ::CreateWindowW(m_WndClass.lpszClassName, m_Title.c_str(), WS_OVERLAPPEDWINDOW, windowX, windowY, props.Width, props.Height, nullptr, nullptr, m_WndClass.hInstance, this);
    ASSERT(m_HMainWnd, "CreateWindow FAILED.");

    SetWindowLongPtr(m_HMainWnd, GWLP_USERDATA, (LONG_PTR)this);

    // client window size is smaller than the whole window size because of the border
    // retrieve the actual client size so that the ImGui layer won't be stretch at first render

    RECT clientRect;
    GetClientRect(m_HMainWnd, &clientRect);
    m_Width = clientRect.right - clientRect.left;
    m_Height = clientRect.bottom - clientRect.top;

    ShowWindow(m_HMainWnd, true);
    UpdateWindow(m_HMainWnd);
}

Window::~Window()
{
    DestroyWindow(m_HMainWnd);
    UnregisterClassW(m_WndClass.lpszClassName, m_WndClass.hInstance);
}

void Window::OnUpdate(Timer &timer)
{
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    CalculateFrameStats(timer);
}

void Window::OnResize(unsigned int width, unsigned int height)
{
    m_Width = width;
    m_Height = height;
}

void Window::CalculateFrameStats(Timer &timer)
{
    // Code computes the average frames per second, and also the
    // average time it takes to render one frame.  These stats
    // are appended to the window caption bar.

    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    // Compute averages over one second period.
    if ((timer.TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = (float)frameCnt; // fps = frameCnt / 1
        float mspf = 1000.0f / fps;

        std::wstring fpsStr = std::to_wstring(fps);
        std::wstring mspfStr = std::to_wstring(mspf);

        std::wstring windowText = m_Title +
                                  L"    fps: " + fpsStr +
                                  L"   mspf: " + mspfStr;

        std::string tmpStr(windowText.begin(), windowText.end());

        SetWindowText(m_HMainWnd, tmpStr.c_str());

        // Reset for next average.
        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    Window *window = (Window *)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    if (!window || !window->Callback)
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg)
    {
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
        window->OnEvent(MouseButtonPressedEvent(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;

    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        window->OnEvent(MouseButtonReleasedEvent(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;

    case WM_MOUSEMOVE:
        window->OnEvent(MouseMovedEvent(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
        return 0;

    case WM_SIZE:
        window->OnEvent(WindowResizeEvent(LOWORD(lParam), HIWORD(lParam)));
        return 0;

    case WM_CLOSE:
        window->OnEvent(WindowCloseEvent());
        return 0;

    case WM_KEYDOWN:
        window->OnEvent(KeyPressedEvent(wParam, LOWORD(lParam)));
        return 0;

    case WM_KEYUP:
        window->OnEvent(KeyReleasedEvent(wParam));
        return 0;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;

    default:
        break;
    }

    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}