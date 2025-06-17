#include "include/ui.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <d3d11.h>
#include <tchar.h>
#include <cmath>
#include <algorithm>

static int deg_num = 20;

// DirectX 11 数据
static ID3D11Device* g_pd3dDeviceDebug = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContextDebug = nullptr;
static IDXGISwapChain* g_pSwapChainDebug = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetViewDebug = nullptr;

// 前向声明
bool CreateDeviceD3DDebug(HWND hWnd);
void CleanupDeviceD3DDebug();
void CreateRenderTargetDebug();
void CleanupRenderTargetDebug();
LRESULT WINAPI WndProcDebug(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

DebugInterface::DebugInterface()
    : showWindow_(true)
    , isCapturing_(false)
    , isPaused_(false)
    , changesApplied_(false)
    , themeId_(0)
    , cursorTime_(0.0f)
    , showCursorValue_(false)
    , selectedVariable_(-1)
    , editorHeight_(300.0f)
    , viewerHeight_(400.0f)
    , controlBarHeight_(60.0f)
    , showEditor_(true)
    , showViewer_(true)
{
    startTime_ = std::chrono::steady_clock::now();
    lastUpdateTime_ = startTime_;
    waveformConfig_.zoomFactor = 1.0f;
}

DebugInterface::~DebugInterface() = default;

void DebugInterface::addEditableVariable(const std::string& name, float* value,
                                        float min, float max, float step,
                                        InputType type) {
    EditableVariable var;
    var.name = name;
    var.value = value;
    var.tempValue = *value;
    var.min = min;
    var.max = max;
    var.step = step;
    var.type = type;

    editableVariables_.push_back(var);
}

void DebugInterface::addWatchVariable(const std::string& name, float* value,
                                     ViewMode mode, const std::string& unit,
                                     unsigned int color) {
    WatchVariable var;
    var.name = name;
    var.valuePtr = value;
    var.mode = mode;
    var.unit = unit;
    var.color = (color == 0xFF00FF00) ? getDefaultColor(watchVariables_.size()) : color;
    var.lastValue = value ? *value : 0.0f;
    var.minValue = var.lastValue;
    var.maxValue = var.lastValue;

    watchVariables_.push_back(var);
}

void DebugInterface::removeEditableVariable(const std::string& name) {
    editableVariables_.erase(
        std::remove_if(editableVariables_.begin(), editableVariables_.end(),
            [&name](const EditableVariable& var) { return var.name == name; }),
        editableVariables_.end()
    );
}

void DebugInterface::removeWatchVariable(const std::string& name) {
    watchVariables_.erase(
        std::remove_if(watchVariables_.begin(), watchVariables_.end(),
            [&name](const WatchVariable& var) { return var.name == name; }),
        watchVariables_.end()
    );
}

void DebugInterface::setWaveformConfig(const WaveformConfig& config) {
    waveformConfig_ = config;
}

void DebugInterface::startCapture() {
    isCapturing_ = true;
    isPaused_ = false;
    startTime_ = std::chrono::steady_clock::now();
    lastUpdateTime_ = startTime_;
}

void DebugInterface::stopCapture() {
    isCapturing_ = false;
    isPaused_ = false;
}

void DebugInterface::pauseCapture() {
    isPaused_ = !isPaused_;
}

bool DebugInterface::isCapturing() const {
    return isCapturing_ && !isPaused_;
}

void DebugInterface::clearHistory() {
    for (auto& var : watchVariables_) {
        var.history.clear();
        if (var.valuePtr) {
            var.minValue = var.maxValue = *var.valuePtr;
        }
    }
}

void DebugInterface::setTheme(int themeId) {
    themeId_ = themeId;
}

void DebugInterface::show() {
    // 创建调试界面窗口
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProcDebug, 0L, 0L,
                      GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr,
                      L"Debug Interface", nullptr };
    ::RegisterClassExW(&wc);

    HWND hwnd = ::CreateWindowW(
        wc.lpszClassName, L"调试界面 - 变量编辑器 & 监控器",
        WS_OVERLAPPEDWINDOW,
        100, 100, 1200, 800,  // 更大的默认窗口
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // 初始化 Direct3D
    if (!CreateDeviceD3DDebug(hwnd)) {
        CleanupDeviceD3DDebug();
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
    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // 启用停靠功能

    // 字体设置
    io.Fonts->Clear();
    bool fontLoaded = false;
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simsun.ttc"
    };

    for (const char* path : fontPaths) {
        if (ImFont* font = io.Fonts->AddFontFromFileTTF(path, 16.0f, nullptr,
                                                        io.Fonts->GetGlyphRangesChineseFull())) {
            fontLoaded = true;
            break;
        }
    }

    if (!fontLoaded) {
        io.Fonts->AddFontDefault();
    }

    // 应用macOS风格
    applyMacOSStyle();

    // 设置平台/渲染器后端
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDeviceDebug, g_pd3dDeviceContextDebug);

    // 主循环
    MSG msg;
    ZeroMemory(&msg, sizeof(msg));
    while (msg.message != WM_QUIT && showWindow_) {
        if (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            continue;
        }

        // 更新数据
        if (isCapturing()) {
            updateData();
        }

        // 开始新帧
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 渲染调试界面
        renderImGui();

        // 渲染
        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.98f, 0.98f, 0.98f, 1.0f };
        g_pd3dDeviceContextDebug->OMSetRenderTargets(1, &g_mainRenderTargetViewDebug, nullptr);
        g_pd3dDeviceContextDebug->ClearRenderTargetView(g_mainRenderTargetViewDebug, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChainDebug->Present(1, 0);
    }

    // 清理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3DDebug();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

void DebugInterface::renderImGui() {
    ImGuiIO& io = ImGui::GetIO();

    // 创建全屏主窗口
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoMove |
                                  ImGuiWindowFlags_NoBringToFrontOnFocus |
                                  ImGuiWindowFlags_NoScrollbar;

    if (ImGui::Begin("##DebugInterface", nullptr, window_flags)) {
        float windowWidth = ImGui::GetWindowSize().x;
        float windowHeight = ImGui::GetWindowSize().y;

        // 标题栏
        float titleWidth = ImGui::CalcTextSize("调试界面").x;
        ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.48f, 0.98f, 1.00f));
        ImGui::Text("调试界面");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // 控制栏
        renderControlBar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // 主要内容区域 - 分栏布局
        float remainingHeight = ImGui::GetContentRegionAvail().y;
        float editorWidth = windowWidth * 0.35f;  // 编辑器占35%宽度
        float viewerWidth = windowWidth * 0.65f;  // 查看器占65%宽度

        // 左侧：变量编辑器
        if (showEditor_) {
            if (ImGui::BeginChild("EditorPanel", ImVec2(editorWidth - 5, remainingHeight), true)) {
                renderEditorPanel();
            }
            ImGui::EndChild();
            ImGui::SameLine();
        }

        // 右侧：变量查看器
        if (showViewer_) {
            if (ImGui::BeginChild("ViewerPanel", ImVec2(viewerWidth - 5, remainingHeight), true)) {
                renderViewerPanel();
            }
            ImGui::EndChild();
        }
    }
    ImGui::End();
}

void DebugInterface::renderControlBar() {
    float windowWidth = ImGui::GetContentRegionAvail().x;

    // 面板显示控制
    ImGui::Checkbox("编辑器", &showEditor_);
    ImGui::SameLine();
    ImGui::Checkbox("监控器", &showViewer_);

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // 数据采集控制按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.42f, 0.85f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 1.00f));
    /*
    if (ImGui::Button(isCapturing_ ? (isPaused_ ? "继续" : "暂停") : "开始", ImVec2(60, 25))) {
        if (!isCapturing_) {
            startCapture();
        } else {
            pauseCapture();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("停止", ImVec2(60, 25))) {
        stopCapture();
    }
    */

    ImGui::SameLine();
    if (ImGui::Button("清空", ImVec2(60, 25))) {
        clearHistory();
    }

    ImGui::PopStyleColor(4);

    // 时间窗口设置
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("时间窗口:");
    ImGui::SameLine();

    static int selectedTimeWindow = 3; // 默认1秒
    const char* timeOptions[] = {"100ms", "200ms", "500ms", "1s", "2s", "5s", "10s", "30s", "60s"};
    const float timeValues[] = {0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f};

    ImGui::SetNextItemWidth(80);
    if (ImGui::Combo("##TimeWindow", &selectedTimeWindow, timeOptions, IM_ARRAYSIZE(timeOptions))) {
        waveformConfig_.timeWindow = static_cast<float>(deg_num) * timeValues[selectedTimeWindow];
    }

    // 选项设置
    ImGui::SameLine();
    ImGui::Checkbox("自动缩放", &waveformConfig_.autoScale);
    ImGui::SameLine();
    ImGui::Checkbox("网格", &waveformConfig_.showGrid);

    // 状态显示
    ImGui::SameLine();
    ImGui::SetCursorPosX(windowWidth - 250);
    ImGui::Text("状态: %s | 缩放: %.1fx | 变量: E%d/W%d",
        isCapturing_ ? (isPaused_ ? "已暂停" : "运行中") : "已停止",
        waveformConfig_.zoomFactor,
        (int)editableVariables_.size(),
        (int)watchVariables_.size());
}

void DebugInterface::renderEditorPanel() {
    // 编辑器标题
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.48f, 0.98f, 1.00f));
    ImGui::Text("变量编辑器");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    if (editableVariables_.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "没有可编辑的变量");
        return;
    }

    // 变量编辑区域
    for (auto& var : editableVariables_) {
        ImGui::PushID(&var);

        // 变量名
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", var.name.c_str());

        // 控件
        float controlWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(controlWidth);

        bool changed = false;
        switch (var.type) {
            case InputType::SLIDER: {
                changed = ImGui::SliderFloat("##slider", &var.tempValue, var.min, var.max, "%.2f");
                break;
            }
            case InputType::DRAG: {
                ImGui::PushStyleVar(ImGuiStyleVar_GrabMinSize, 8.0f);
                changed = ImGui::DragFloat("##drag", &var.tempValue, var.step, var.min, var.max, "%.2f");
                ImGui::PopStyleVar();
                break;
            }
            case InputType::INPUT_BOX: {
                changed = ImGui::InputFloat("##input", &var.tempValue, var.step, var.step * 10.0f, "%.2f");
                if (changed) {
                    var.tempValue = std::max(var.min, std::min(var.tempValue, var.max));
                }
                break;
            }
        }

        // 范围信息
        ImGui::PushStyleColor(ImGuiCol_TextDisabled, ImVec4(0.50f, 0.50f, 0.50f, 1.00f));
        ImGui::TextDisabled("[%.1f - %.1f]", var.min, var.max);
        ImGui::PopStyleColor();

        ImGui::PopID();
        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 应用按钮
    float buttonWidth = 120;
    float buttonHeight = 28;
    ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - buttonWidth) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.48f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.10f, 0.53f, 0.98f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.00f, 0.42f, 0.85f, 1.00f));
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
            float msgWidth = ImGui::CalcTextSize("已应用").x;
            ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - msgWidth) * 0.5f);
            ImGui::TextColored(ImVec4(0.00f, 0.65f, 0.00f, 1.00f), "已应用");
        } else {
            changesApplied_ = false;
            messageTimer = 0.0f;
        }
    }
}

void DebugInterface::renderViewerPanel() {
    // 查看器标题
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.00f, 0.48f, 0.98f, 1.00f));
    ImGui::Text("变量监控器");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    if (watchVariables_.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "没有监控变量");
        return;
    }

    float remainingHeight = ImGui::GetContentRegionAvail().y;
    float waveformHeight = remainingHeight * 0.6f;  // 波形占60%
    float watchHeight = remainingHeight * 0.4f;     // Watch占40%

    // 波形显示区域
    if (ImGui::BeginChild("WaveformArea", ImVec2(0, waveformHeight), true)) {
        renderWaveformArea();
    }
    ImGui::EndChild();

    // Watch窗口
    if (ImGui::BeginChild("WatchArea", ImVec2(0, watchHeight), true)) {
        renderWatchArea();
    }
    ImGui::EndChild();
}

void DebugInterface::renderWaveformArea() {
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();

    // 为图例预留空间
    float legendHeight = 50.0f;
    canvas_sz.y -= legendHeight;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    // 绘制背景
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(250, 250, 250, 255));
    draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(200, 200, 200, 255));

    // 绘制网格
    if (waveformConfig_.showGrid) {
        drawGrid();
    }

    // 计算Y轴范围
    float minY = 0, maxY = 1;
    if (waveformConfig_.autoScale && !watchVariables_.empty()) {
        bool first = true;
        for (const auto& var : watchVariables_) {
            if (var.mode == ViewMode::WAVEFORM && var.visible && !var.history.empty()) {
                if (first) {
                    minY = var.minValue;
                    maxY = var.maxValue;
                    first = false;
                } else {
                    minY = std::min(minY, var.minValue);
                    maxY = std::max(maxY, var.maxValue);
                }
            }
        }
        if (maxY == minY) {
            maxY = minY + 1;
        }
    } else {
        minY = waveformConfig_.minY;
        maxY = waveformConfig_.maxY;
    }

    // 绘制波形
    float currentTime = getCurrentTime();
    for (const auto& var : watchVariables_) {
        if (var.mode == ViewMode::WAVEFORM && var.visible && !var.history.empty()) {
            std::vector<float> times, values;

            float timeStart = currentTime - waveformConfig_.timeWindow * waveformConfig_.zoomFactor;
            for (const auto& point : var.history) {
                if (point.timestamp >= timeStart) {
                    times.push_back(point.timestamp);
                    values.push_back(point.value);
                }
            }

            if (!times.empty()) {
                drawWaveform(var, times, values, var.color);
            }
        }
    }

    // 绘制游标
    if (waveformConfig_.showCursor) {
        drawCursor();
    }

    // 处理鼠标交互
    ImGui::InvisibleButton("canvas", canvas_sz);
    if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
        ImGuiIO& io = ImGui::GetIO();

        // 鼠标滚轮缩放
        if (io.MouseWheel != 0.0f) {
            float zoomDelta = io.MouseWheel * 0.1f;
            waveformConfig_.zoomFactor = std::max(0.1f, std::min(10.0f, waveformConfig_.zoomFactor - zoomDelta));
        }

        // 鼠标点击设置游标
        if (io.MouseClicked[0]) {
            float mouseX = io.MousePos.x - canvas_p0.x;
            cursorTime_ = currentTime - waveformConfig_.timeWindow * waveformConfig_.zoomFactor +
                         (mouseX / canvas_sz.x) * waveformConfig_.timeWindow * waveformConfig_.zoomFactor;
            showCursorValue_ = true;
        }
    }

    // 移动到图例区域
    ImGui::SetCursorScreenPos(ImVec2(canvas_p0.x, canvas_p1.y + 5));

    // 精简的图例
    ImGui::Text("图例:");
    ImGui::SameLine();

    int visibleCount = 0;
    for (size_t i = 0; i < watchVariables_.size(); ++i) {
        auto& var = watchVariables_[i];
        if (var.mode == ViewMode::WAVEFORM) {
            if (visibleCount > 0 && visibleCount % 3 == 0) {
                ImGui::NewLine();
                ImGui::Text("     ");
                ImGui::SameLine();
            } else if (visibleCount > 0) {
                ImGui::SameLine();
            }

            ImGui::PushID(i);

            ImVec4 color = ImGui::ColorConvertU32ToFloat4(var.color);
            ImGui::ColorButton("##color", color, ImGuiColorEditFlags_NoTooltip, ImVec2(10, 10));

            ImGui::SameLine();
            ImGui::Checkbox(var.name.c_str(), &var.visible);

            ImGui::PopID();
            visibleCount++;
        }
    }
}

void DebugInterface::renderWatchArea() {
    // Watch窗口标题
    ImGui::Text("Watch 窗口");
    ImGui::Separator();
    ImGui::Spacing();

    // 检查是否有数值变量
    bool hasNumericVars = false;
    for (const auto& var : watchVariables_) {
        if (var.mode == ViewMode::NUMERIC) {
            hasNumericVars = true;
            break;
        }
    }

    if (!hasNumericVars) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "没有数值变量需要监控");
        return;
    }

    // 表格显示
    if (ImGui::BeginTable("WatchTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("变量名", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("当前值", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("最小值", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("最大值", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("单位", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (auto& var : watchVariables_) {
            if (var.mode == ViewMode::NUMERIC) {
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", var.name.c_str());

                ImGui::TableSetColumnIndex(1);
                if (var.valuePtr) {
                    float currentValue = *var.valuePtr;
                    ImVec4 valueColor = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
                    if (currentValue > var.lastValue) {
                        valueColor = ImVec4(0.0f, 0.7f, 0.0f, 1.0f);
                    } else if (currentValue < var.lastValue) {
                        valueColor = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);
                    }
                    ImGui::TextColored(valueColor, "%.3f", currentValue);
                } else {
                    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "N/A");
                }

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", var.minValue);

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", var.maxValue);

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%s", var.unit.c_str());
            }
        }

        ImGui::EndTable();
    }

    // 游标信息
    if (showCursorValue_) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Text("游标信息 (%.3fs):", cursorTime_);

        for (const auto& var : watchVariables_) {
            if (var.mode == ViewMode::WAVEFORM && var.visible && !var.history.empty()) {
                float closestValue = 0.0f;
                float minTimeDiff = FLT_MAX;
                bool found = false;

                for (const auto& point : var.history) {
                    float timeDiff = std::abs(point.timestamp - cursorTime_);
                    if (timeDiff < minTimeDiff) {
                        minTimeDiff = timeDiff;
                        closestValue = point.value;
                        found = true;
                    }
                }

                if (found) {
                    ImVec4 color = ImGui::ColorConvertU32ToFloat4(var.color);
                    ImGui::TextColored(color, "%s: %.3f %s", var.name.c_str(), closestValue, var.unit.c_str());
                }
            }
        }
    }
}

void DebugInterface::applyChanges() {
    for (auto& var : editableVariables_) {
        *var.value = var.tempValue;
    }
}

void DebugInterface::updateData() {
    auto currentTime = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(currentTime - lastUpdateTime_).count();

    if (isPaused_) {
        return; // 不添加新数据，也不更新时间
    }

    if (deltaTime < 1.0f / waveformConfig_.refreshRate) {
        return;
    }

    float timestamp = getCurrentTime();
    float retentionTime = waveformConfig_.timeWindow * waveformConfig_.zoomFactor * 2.0f;

    for (auto& var : watchVariables_) {
        if (var.valuePtr) {
            float currentValue = *var.valuePtr;

            // 更新最大最小值和最后的值
            var.minValue = std::min(var.minValue, currentValue);
            var.maxValue = std::max(var.maxValue, currentValue);
            var.lastValue = currentValue;

            if (var.mode == ViewMode::WAVEFORM) {
                // 总是添加新数据
                var.history.push_back({timestamp, currentValue});

                // 只在非暂停状态下清理旧数据
                if (!isPaused_ && var.history.size() > 2) {
                    while (!var.history.empty() &&
                           var.history.front().timestamp < timestamp - retentionTime) {
                        var.history.erase(var.history.begin());
                           }
                }
            }
        }
    }

    lastUpdateTime_ = currentTime;
}

void DebugInterface::drawWaveform(const WatchVariable& var, const std::vector<float>& times,
                                const std::vector<float>& values, unsigned int color) {
    if (times.size() < 2) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    canvas_sz.y -= 50.0f;

    // 计算时间范围
    float timeEnd;
    float timeStart;

    if (isPaused_) {
        // 第一次暂停时记录
        if (pausedTimeEnd_ == 0.0f) {
            pausedTimeEnd_ = times.back();
            pausedTimeStart_ = pausedTimeEnd_ - waveformConfig_.timeWindow * waveformConfig_.zoomFactor;
        }
        timeStart = pausedTimeStart_;
        timeEnd = pausedTimeEnd_;
    } else {
        timeEnd = getCurrentTime();
        timeStart = timeEnd - waveformConfig_.timeWindow * waveformConfig_.zoomFactor;

        // 清除暂停时间
        pausedTimeStart_ = 0.0f;
        pausedTimeEnd_ = 0.0f;
    }


    float minY = waveformConfig_.autoScale ? var.minValue : waveformConfig_.minY;
    float maxY = waveformConfig_.autoScale ? var.maxValue : waveformConfig_.maxY;
    if (maxY == minY) maxY = minY + 1;

    // 绘制所有数据点
    for (size_t i = 1; i < times.size(); ++i) {
        float x1 = canvas_p0.x + ((times[i-1] - timeStart) / (timeEnd - timeStart)) * canvas_sz.x;
        float y1 = canvas_p0.y + canvas_sz.y - ((values[i-1] - minY) / (maxY - minY)) * canvas_sz.y;
        float x2 = canvas_p0.x + ((times[i] - timeStart) / (timeEnd - timeStart)) * canvas_sz.x;
        float y2 = canvas_p0.y + canvas_sz.y - ((values[i] - minY) / (maxY - minY)) * canvas_sz.y;

        draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, 2.0f);
    }
}

void DebugInterface::drawGrid() {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    canvas_sz.y -= 50.0f;
    ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

    const int gridLines = deg_num;
    ImU32 gridColor = IM_COL32(220, 220, 220, 255);

    for (int i = 1; i < gridLines; ++i) {
        float x = canvas_p0.x + (canvas_sz.x / gridLines) * i;
        draw_list->AddLine(ImVec2(x, canvas_p0.y), ImVec2(x, canvas_p1.y), gridColor);
    }

    for (int i = 1; i < gridLines; ++i) {
        float y = canvas_p0.y + (canvas_sz.y / gridLines) * i;
        draw_list->AddLine(ImVec2(canvas_p0.x, y), ImVec2(canvas_p1.x, y), gridColor);
    }
}

void DebugInterface::drawCursor() {
    if (!showCursorValue_) return;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();
    ImVec2 canvas_sz = ImGui::GetContentRegionAvail();
    canvas_sz.y -= 50.0f;

    float currentTime = getCurrentTime();
    float timeStart = currentTime - waveformConfig_.timeWindow * waveformConfig_.zoomFactor;
    float timeEnd = currentTime;

    float cursorX = canvas_p0.x + ((cursorTime_ - timeStart) / (timeEnd - timeStart)) * canvas_sz.x;

    if (cursorX >= canvas_p0.x && cursorX <= canvas_p0.x + canvas_sz.x) {
        draw_list->AddLine(ImVec2(cursorX, canvas_p0.y),
                          ImVec2(cursorX, canvas_p0.y + canvas_sz.y),
                          IM_COL32(255, 0, 0, 255), 2.0f);
    }
}

float DebugInterface::getCurrentTime() const {
    auto currentTime = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(currentTime - startTime_).count();
}

void DebugInterface::applyMacOSStyle() {
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowPadding = ImVec2(15, 15);
    style.ItemSpacing = ImVec2(8, 8);
    style.FramePadding = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.ScrollbarSize = 12.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.98f, 0.99f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.92f, 0.94f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.00f, 0.48f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.10f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.00f, 0.42f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.00f, 0.48f, 0.98f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.10f, 0.53f, 0.98f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.88f, 0.88f, 0.88f, 0.60f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.00f, 0.48f, 0.98f, 1.00f);
}

unsigned int DebugInterface::getDefaultColor(int index) {
    const unsigned int colors[] = {
        IM_COL32(0, 120, 250, 255),
        IM_COL32(250, 60, 60, 255),
        IM_COL32(0, 200, 80, 255),
        IM_COL32(250, 150, 0, 255),
        IM_COL32(150, 0, 200, 255),
        IM_COL32(0, 180, 180, 255),
        IM_COL32(250, 200, 0, 255),
        IM_COL32(200, 100, 100, 255),
    };

    return colors[index % (sizeof(colors) / sizeof(colors[0]))];
}

// DirectX 11 函数实现
bool CreateDeviceD3DDebug(HWND hWnd) {
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
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
                                     featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChainDebug,
                                     &g_pd3dDeviceDebug, &featureLevel, &g_pd3dDeviceContextDebug) != S_OK)
        return false;

    CreateRenderTargetDebug();
    return true;
}

void CleanupDeviceD3DDebug() {
    CleanupRenderTargetDebug();
    if (g_pSwapChainDebug) { g_pSwapChainDebug->Release(); g_pSwapChainDebug = nullptr; }
    if (g_pd3dDeviceContextDebug) { g_pd3dDeviceContextDebug->Release(); g_pd3dDeviceContextDebug = nullptr; }
    if (g_pd3dDeviceDebug) { g_pd3dDeviceDebug->Release(); g_pd3dDeviceDebug = nullptr; }
}

void CreateRenderTargetDebug() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChainDebug->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDeviceDebug->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetViewDebug);
    pBackBuffer->Release();
}

void CleanupRenderTargetDebug() {
    if (g_mainRenderTargetViewDebug) { g_mainRenderTargetViewDebug->Release(); g_mainRenderTargetViewDebug = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT WINAPI WndProcDebug(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDeviceDebug != nullptr && wParam != SIZE_MINIMIZED) {
            CleanupRenderTargetDebug();
            g_pSwapChainDebug->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTargetDebug();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}