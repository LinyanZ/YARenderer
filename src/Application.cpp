#include "Application.h"
#include <imgui.h>
#include "rendering/RenderingSettings.h"

RenderingSettings g_RenderingSettings;

Application::Application()
{
    m_Window = std::make_unique<Window>();
    m_Window->Callback = std::bind(&Application::OnEvent, this, std::placeholders::_1);

    m_DxContext = make_ref<DxContext>(m_Window->GetHandle(), m_Window->GetWidth(), m_Window->GetHeight());
    m_Renderer = std::make_unique<Renderer>(m_DxContext, m_Window->GetWidth(), m_Window->GetHeight());
    m_UI = std::make_unique<UI>(m_DxContext, m_Window->GetHandle());
}

Application::~Application()
{
    m_DxContext->Flush();
}

void Application::Run()
{
    m_Renderer->Setup();

    Timer.Reset();

    while (Running)
    {
        Timer.Tick();

        m_Window->OnUpdate(Timer);
        m_Renderer->OnUpdate(Timer);

        m_UI->BeginFrame();
        m_UI->Render();

        m_Renderer->BeginFrame();
        m_Renderer->Render();

        m_UI->EndFrame();
        m_Renderer->EndFrame();
    }
}

void Application::OnEvent(Event &e)
{
    switch (e.GetEventType())
    {
    case EventType::WindowClose:
        Running = false;
        break;
    case EventType::WindowResize:
        OnResize(dynamic_cast<WindowResizeEvent &>(e));
        break;
    case EventType::MouseButtonPressed:
        OnMousePressed(dynamic_cast<MouseButtonPressedEvent &>(e));
        break;
    case EventType::MouseButtonReleased:
        OnMouseReleased(dynamic_cast<MouseButtonReleasedEvent &>(e));
        break;
    case EventType::MouseMoved:
        OnMouseMoved(dynamic_cast<MouseMovedEvent &>(e));
        break;
    case EventType::KeyPressed:
        OnKeyPressed(dynamic_cast<KeyPressedEvent &>(e));
        break;
    case EventType::KeyReleased:
        OnKeyReleased(dynamic_cast<KeyReleasedEvent &>(e));
        break;
    default:
        break;
    }
}

void Application::OnResize(WindowResizeEvent &e)
{
    m_Window->OnResize(e.GetWidth(), e.GetHeight());
    m_DxContext->OnResize(e.GetWidth(), e.GetHeight());
    m_Renderer->OnResize(e.GetWidth(), e.GetHeight());
}

void Application::OnMousePressed(MouseButtonPressedEvent &e)
{
    m_LastMousePosX = e.GetX();
    m_LastMousePosY = e.GetY();

    SetCapture(m_Window->GetHandle());
}

void Application::OnMouseReleased(MouseButtonReleasedEvent &e)
{
    ReleaseCapture();
}

void Application::OnMouseMoved(MouseMovedEvent &e)
{
    auto &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
        return;

    if ((e.GetBtnState() & MK_LBUTTON))
    {
        m_Renderer->OnMouseInput(e.GetX() - m_LastMousePosX, e.GetY() - m_LastMousePosY);

        m_LastMousePosX = e.GetX();
        m_LastMousePosY = e.GetY();
    }
}

void Application::OnKeyPressed(KeyPressedEvent &e)
{
    if (e.GetKeyCode() == VK_ESCAPE)
        Running = false;
}

void Application::OnKeyReleased(KeyReleasedEvent &e)
{
}