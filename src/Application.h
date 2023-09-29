#pragma once

#include "pch.h"
#include "rendering/Renderer.h"
#include "core/Window.h"
#include "core/UI.h"
#include "core/Timer.h"
#include "dx/DxContext.h"

#include "event/Event.h"
#include "event/ApplicationEvent.h"
#include "event/MouseEvent.h"
#include "event/KeyEvent.h"

class Application
{
public:
    Application();
    ~Application();

    void Run();

    void OnEvent(Event &e);

    void OnResize(WindowResizeEvent &e);

    void OnMousePressed(MouseButtonPressedEvent &e);
    void OnMouseReleased(MouseButtonReleasedEvent &e);
    void OnMouseMoved(MouseMovedEvent &e);

    void OnKeyPressed(KeyPressedEvent &e);
    void OnKeyReleased(KeyReleasedEvent &e);

    bool Running = true;
    Timer Timer;

private:
    std::unique_ptr<Window> m_Window;
    std::unique_ptr<Renderer> m_Renderer;
    std::unique_ptr<UI> m_UI;

    Ref<DxContext> m_DxContext;

    int m_LastMousePosX = 0;
    int m_LastMousePosY = 0;
};
