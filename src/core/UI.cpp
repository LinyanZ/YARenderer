#include "UI.h"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include "rendering/RenderingSettings.h"

extern RenderingSettings g_RenderingSettings;

UI::UI(Ref<DxContext> dxContext, HWND hwnd)
    : m_DxContext(dxContext)
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;   // Enable Docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    ImGui::StyleColorsDark();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle &style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    SetDarkThemeColors();
    io.FontDefault = io.Fonts->AddFontFromFileTTF("resources/fonts/opensans/OpenSans-Regular.ttf", 18.f);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX12_Init(dxContext->GetDevice().Get(), NUM_FRAMES_IN_FLIGHT,
                        BACK_BUFFER_FORMAT, dxContext->GetImGuiHeap().Get(),
                        dxContext->GetImGuiHeap().Get()->GetCPUDescriptorHandleForHeapStart(),
                        dxContext->GetImGuiHeap().Get()->GetGPUDescriptorHandleForHeapStart());
}

UI::~UI()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void UI::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void UI::Render()
{
    // ImGui::ShowDemoWindow();

    if (!ImGui::Begin("Rendering Settings"))
    {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
        return;
    }

    ImGui::PushItemWidth(-200);

    if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("VSync", &g_RenderingSettings.EnableVSync);
    }

    if (ImGui::CollapsingHeader("Graphics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::TreeNodeEx("Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SeparatorText("Sun");
            ImGui::DragFloat("Sun Theta Angle (Degree)", &g_RenderingSettings.SunTheta, 0.1, 0, 360);
            ImGui::DragFloat("Sun Phi Angle (Degree)", &g_RenderingSettings.SunPhi, 0.1, 0, 90);
            ImGui::SliderFloat("Sun Light Intensity", &g_RenderingSettings.SunLightIntensity, 0.1, 20);

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Shadow"))
        {
            ImGui::SeparatorText("General");
            ImGui::SliderFloat("Max Shadow Distance", &g_RenderingSettings.MaxShadowDistance, 10.0f, 500.0f, "%.3f");
            ImGui::Checkbox("Show Cascades", &g_RenderingSettings.ShowCascades);

            ImGui::SeparatorText("Cascade Shadow");
            ImGui::SliderFloat("Cascade Range Scale", &g_RenderingSettings.CascadeRangeScale, 1.0f, 5.0f, "%.3f");
            ImGui::SliderFloat("Cascade Transition Ratio", &g_RenderingSettings.CascadeTransitionRatio, 0.0f, 0.5f, "%.3f");

            ImGui::SeparatorText("PCSS");
            ImGui::SliderFloat("Shadow Softness", &g_RenderingSettings.ShadowSoftness, 0.0f, 1.0f, "%.3f");
            ImGui::Checkbox("Use Vogel Disk Sample", &g_RenderingSettings.UseVogelDiskSample);
            ImGui::SliderInt("Sample Count", &g_RenderingSettings.NumSamples, 1, 64);

            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("VXGI", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enable GI", &g_RenderingSettings.GI.Enable);
            ImGui::Checkbox("Dynamic Update", &g_RenderingSettings.GI.DynamicUpdate);
            ImGui::Checkbox("Second Bounce", &g_RenderingSettings.GI.SecondBounce);
            ImGui::Checkbox("Debug Voxel", &g_RenderingSettings.GI.DebugVoxel);
            ImGui::SliderInt("Debug Voxel Mip Level", &g_RenderingSettings.GI.DebugVoxelMipLevel, 0, 7);

            ImGui::TreePop();
        }
    }

    if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::TreeNode("Motion Blur"))
        {
            ImGui::Checkbox("Enable", &g_RenderingSettings.EnableMotionBlur);
            ImGui::SliderFloat("Amount", &g_RenderingSettings.MotionBlurAmount, 0.0f, 2.0f, "%.3f");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Antialiasing"))
        {
            ImGui::Combo("Method", &(int &)g_RenderingSettings.AntialisingMethod, "None\0FXAA\0TAA\0\0");

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Tone Mapping"))
        {
            ImGui::Checkbox("Enable", &g_RenderingSettings.EnableToneMapping);
            ImGui::SliderFloat("Exposure", &g_RenderingSettings.Exposure, 0.0f, 2.0f, "%.3f");
            ImGui::TreePop();
        }
    }

    ImGui::End();
}

void UI::EndFrame()
{
    auto commandList = m_DxContext->GetCommandList();
    commandList->SetDescriptorHeaps(1, m_DxContext->GetImGuiHeap().GetAddressOf());
    commandList->OMSetRenderTargets(1, &m_DxContext->CurrentBackBuffer().Rtv.CPUHandle, true, nullptr);

    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.Get());

    // Update and Render additional Platform Windows
    ImGuiIO &io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault(nullptr, (void *)commandList.Get());
    }
}

void UI::SetDarkThemeColors()
{
    auto &colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{0.1f, 0.105f, 0.11f, 1.0f};

    // Headers
    colors[ImGuiCol_Header] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_HeaderHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_HeaderActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Buttons
    colors[ImGuiCol_Button] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_ButtonHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_ButtonActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Frame BG
    colors[ImGuiCol_FrameBg] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};
    colors[ImGuiCol_FrameBgHovered] = ImVec4{0.3f, 0.305f, 0.31f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TabHovered] = ImVec4{0.38f, 0.3805f, 0.381f, 1.0f};
    colors[ImGuiCol_TabActive] = ImVec4{0.28f, 0.2805f, 0.281f, 1.0f};
    colors[ImGuiCol_TabUnfocused] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{0.2f, 0.205f, 0.21f, 1.0f};

    // Title
    colors[ImGuiCol_TitleBg] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{0.15f, 0.1505f, 0.151f, 1.0f};
}