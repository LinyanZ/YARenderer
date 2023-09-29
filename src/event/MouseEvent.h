#pragma once
#include "Event.h"

class MouseMovedEvent : public Event
{
public:
    MouseMovedEvent(int btnState, int x, int y)
        : m_BtnState(btnState), m_MouseX(x), m_MouseY(y) {}

    inline int GetBtnState() const { return m_BtnState; }
    inline int GetX() const { return m_MouseX; }
    inline int GetY() const { return m_MouseY; }

    std::string ToString() const override
    {
        std::stringstream ss;
        ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseMoved)

private:
    int m_BtnState, m_MouseX, m_MouseY;
};

class MouseScrolledEvent : public Event
{
public:
    MouseScrolledEvent(float xOffset, float yOffset)
        : m_XOffset(xOffset), m_YOffset(yOffset) {}

    inline float GetXOffset() const { return m_XOffset; }
    inline float GetYOffset() const { return m_YOffset; }

    std::string ToString() const override
    {
        std::stringstream ss;
        ss << "MouseScrolledEvent: " << GetXOffset() << ", " << GetYOffset();
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseScrolled)

private:
    float m_XOffset, m_YOffset;
};

class MouseButtonEvent : public Event
{
public:
    inline int GetMouseButton() const { return m_Button; }
    inline int GetX() const { return m_MouseX; }
    inline int GetY() const { return m_MouseY; }

protected:
    MouseButtonEvent(int button, int x, int y)
        : m_Button(button), m_MouseX(x), m_MouseY(y) {}

    int m_Button;
    int m_MouseX, m_MouseY;
};

class MouseButtonPressedEvent : public MouseButtonEvent
{
public:
    MouseButtonPressedEvent(int button, int x, int y)
        : MouseButtonEvent(button, x, y) {}

    std::string ToString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonPressedEvent: " << m_Button;
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonPressed)
};

class MouseButtonReleasedEvent : public MouseButtonEvent
{
public:
    MouseButtonReleasedEvent(int button, int x, int y)
        : MouseButtonEvent(button, x, y) {}

    std::string ToString() const override
    {
        std::stringstream ss;
        ss << "MouseButtonReleasedEvent: " << m_Button;
        return ss.str();
    }

    EVENT_CLASS_TYPE(MouseButtonReleased)
};