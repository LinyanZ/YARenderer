#pragma once

#include "pch.h"

enum class EventType
{
    None = 0,
    WindowClose,
    WindowResize,
    WindowFocus,
    WindowLostFocus,
    WindowMoved,
    AppTick,
    AppUpdate,
    AppRender,
    KeyPressed,
    KeyReleased,
    KeyTyped,
    MouseButtonPressed,
    MouseButtonReleased,
    MouseMoved,
    MouseScrolled
};

class Event
{
public:
    virtual EventType GetEventType() const = 0;
    virtual const char *GetName() const = 0;
    virtual std::string ToString() const { return GetName(); };

    bool Handled = false;
};

#define EVENT_CLASS_TYPE(type)                                                    \
    virtual EventType GetEventType() const override { return EventType::##type; } \
    virtual const char *GetName() const override { return #type; }

inline std::ostream &operator<<(std::ostream &os, const Event &e)
{
    return os << e.ToString();
}
