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
    // 创建应用窗口
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Variable Editor", nullptr };
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"变量编辑器", WS_OVERLAPPEDWINDOW, 100, 100, 400, 400, nullptr, nullptr, wc.hInstance, nullptr);

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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // 启用键盘控制
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // 启用停靠功能

    // 设置 ImGui 风格
    switch (themeId_) {
        case 0:
            ImGui::StyleColorsDark();
            break;
        case 1:
            ImGui::StyleColorsLight();
            break;
        default:
            ImGui::StyleColorsClassic();
            break;
    }

    // 设置平台/渲染器后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // 应用自定义样式
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.TabRounding = 4.0f;
    style.WindowPadding = ImVec2(10, 10);
    style.ItemSpacing = ImVec2(8, 8);
    style.FramePadding = ImVec2(8, 4);

    // 设置颜色
    if (themeId_ == 0) {  // 深色主题增强
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.94f);
        colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 0.55f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 0.80f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.24f, 0.40f, 0.95f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.33f, 0.50f, 0.95f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.21f, 0.30f, 0.90f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.24f, 0.40f, 0.95f, 1.00f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.33f, 0.50f, 0.95f, 1.00f);
    }

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

        // 渲染编辑器窗口
        renderImGui();

        // 渲染
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // 垂直同步
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
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("变量编辑器", &showWindow_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("调整变量值");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool valuesChanged = false;

        for (auto& var : variables_) {
            ImGui::PushID(&var);

            // 显示变量名称
            ImGui::Text("%s", var.name.c_str());

            // 根据类型创建不同的控件
            bool changed = false;

            if (var.type == InputType::SLIDER) {
                changed = ImGui::SliderFloat("##slider", &var.tempValue, var.min, var.max, "%.2f");
            } else if (var.type == InputType::DRAG) {
                changed = ImGui::DragFloat("##drag", &var.tempValue, var.step, var.min, var.max, "%.2f");
            } else if (var.type == InputType::INPUT_BOX) {
                changed = ImGui::InputFloat("##input", &var.tempValue, var.step, var.step * 10.0f, "%.2f");

                // 限制范围
                if (changed) {
                    var.tempValue = std::max(var.min, std::min(var.tempValue, var.max));
                }
            }

            // 显示范围信息
            ImGui::SameLine();
            ImGui::TextDisabled("[%.1f - %.1f]", var.min, var.max);

            if (changed) {
                valuesChanged = true;
            }

            ImGui::PopID();
            ImGui::Spacing();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 应用按钮
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 120) * 0.5f);
        if (ImGui::Button("应用修改", ImVec2(120, 40))) {
            applyChanges();
            changesApplied_ = true;
        }

        // 显示应用成功消息
        if (changesApplied_) {
            static float messageTimer = 0.0f;
            messageTimer += ImGui::GetIO().DeltaTime;

            if (messageTimer < 2.0f) {
                ImGui::Spacing();
                ImGui::SetCursorPosX((ImGui::GetWindowWidth() - ImGui::CalcTextSize("✓ 修改已应用").x) * 0.5f);
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ 修改已应用");
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
