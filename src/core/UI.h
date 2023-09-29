#pragma once

#include "dx/DxContext.h"

class UI
{
public:
    UI(Ref<DxContext> dxContext, HWND hwnd);
    ~UI();

    void BeginFrame();
    void Render();
    void EndFrame();

private:
    void SetDarkThemeColors();

private:
    Ref<DxContext> m_DxContext;
};