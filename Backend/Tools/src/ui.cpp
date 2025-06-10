#include "include/ui.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <cmath>

// DirectX 11 数据
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// 前向声明
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

VariableEditor::VariableEditor() : showWindow_(true), changesApplied_(false), themeId_(0) {
}

VariableEditor::~VariableEditor() {
}

void VariableEditor::addVariable(const std::string& name, float* value,
                               float min, float max, float step,
                               InputType type) {
    Variable var;
    var.name = name;
    var.value = value;
    var.tempValue = *value;
    var.min = min;
    var.max = max;
    var.step = step;
    var.type = type;

    variables_.push_back(var);
}

void VariableEditor::setTheme(int themeId) {
    themeId_ = themeId;
}

void VariableEditor::show() {
    // 创建可拖动的窗口
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Variable Editor", nullptr };
    ::RegisterClassExW(&wc);

    // 创建普通窗口，带有标准边框和标题栏，可以拖动
    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"变量编辑器",
        WS_OVERLAPPEDWINDOW,  // 使用标准窗口样式，包含标题栏和边框
        100, 100, 400, 400,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // 初始化 Direct3D
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return;
    }

    // 显示窗口
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // 设置 ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // 尝试加载黑体字体
    io.Fonts->Clear();
    bool fontLoaded = false;
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc"
    };

    for (const char* path : fontPaths) {
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, 16.0f, nullptr, io.Fonts->GetGlyphRangesChineseFull())) {
            fontLoaded = true;
            break;
        }
    }

    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
    }

    // 设置风格
    ImGui::StyleColorsDark();

    // 应用苹果风格
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;  // 不需要圆角，因为外部窗口已经有边框
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 5.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.ItemSpacing = ImVec2(8, 10);
    style.FramePadding = ImVec2(10, 6);

    // 苹果风格配色
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 0.98f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.93f, 0.94f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.91f, 0.92f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.87f, 0.88f, 0.89f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.00f, 0.45f, 0.90f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.50f, 0.95f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.40f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.45f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.50f, 0.95f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.45f, 0.90f, 1.00f);

    // 设置平台/渲染器后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 主循环
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT && showWindow_) {
        if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // 开始新帧
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 渲染编辑器内容 - 使用无边框窗口填满整个客户区
        renderImGui();

        // 渲染
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.98f, 0.98f, 0.98f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0);
    }

    // 清理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

void VariableEditor::renderImGui() {
    // 获取窗口尺寸
    ImGuiIO& io = ImGui::GetIO();

    // 创建一个无边框窗口，填满整个客户区
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##Content", nullptr, window_flags)) {
        float windowWidth = ImGui::GetWindowSize().x;

        // 内容标题
        float titleWidth = ImGui::CalcTextSize("变量编辑器").x;
        ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.45f, 0.90f, 1.00f));
        ImGui::Text("变量编辑器");
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 变量编辑部分
        for (auto& var : variables_) {
            ImGui::PushID(&var);

            // 变量名
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", var.name.c_str());

            ImGui::SameLine();

            // 控件
            float controlWidth = ImGui::GetContentRegionAvail().x * 0.7f;
            ImGui::SetNextItemWidth(controlWidth);

            bool changed = false;
            switch (var.type) {
                case InputType::SLIDER:
                    changed = ImGui::SliderFloat("##slider", &var.tempValue, var.min, var.max, "%.2f");
                    break;
                case InputType::DRAG:
                    changed = ImGui::DragFloat("##drag", &var.tempValue, var.step, var.min, var.max, "%.2f");
                    break;
                case InputType::INPUT_BOX:
                    changed = ImGui::InputFloat("##input", &var.tempValue, var.step, var.step * 10.0f, "%.2f");
                    if (changed) {
                        var.tempValue = std::max(var.min, std::min(var.tempValue, var.max));
                    }
                    break;
            }

            // 范围信息
            ImGui::SameLine();
            ImGui::TextDisabled("[%.1f-%.1f]", var.min, var.max);

            ImGui::PopID();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 应用按钮
        float buttonWidth = 120;
        float buttonHeight = 30;
        ImGui::SetCursorPosX((windowWidth - buttonWidth) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.45f, 0.90f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.00f, 0.50f, 0.95f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.40f, 0.85f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        if (ImGui::Button("应用修改", ImVec2(buttonWidth, buttonHeight))) {
            applyChanges();
            changesApplied_ = true;
        }

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(4);

        // 显示应用成功消息
        if (changesApplied_) {
            static float messageTimer = 0.0f;
            messageTimer += ImGui::GetIO().DeltaTime;

            if (messageTimer < 1.5f) {
                ImGui::Spacing();
                float msgWidth = ImGui::CalcTextSize("✓ 已应用").x;
                ImGui::SetCursorPosX((windowWidth - msgWidth) * 0.5f);
                ImGui::TextColored(ImVec4(0.00f, 0.60f, 0.00f, 1.00f), "✓ 已应用");
            } else {
                changesApplied_ = false;
                messageTimer = 0.0f;
            }
        }
    }
    ImGui::End();
}


void VariableEditor::applyChanges() {
    for (auto& var : variables_) {
        *var.value = var.tempValue;
    }
}

// 创建 Direct3D 设备
bool CreateDeviceD3D(HWND hWnd) {
    // 设置交换链
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext) != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Win32 消息处理
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // 禁用 ALT 应用程序菜单
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
