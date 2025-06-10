#include "include/ui.h"
#include <windowsx.h>
#include <commctrl.h>
#include <string>
#include <sstream>

#pragma comment(lib, "comctl32.lib")

VariableEditor* VariableEditor::instance_ = nullptr;

VariableEditor::VariableEditor() : hwnd_(nullptr), font_(nullptr) {
    instance_ = this;

    // 初始化Common Controls
    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icc.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);
}

VariableEditor::~VariableEditor() {
    if (font_) DeleteObject(font_);
    if (hwnd_) DestroyWindow(hwnd_);
    instance_ = nullptr;
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
    var.control = nullptr;
    var.label = nullptr;

    variables_.push_back(var);
}

void VariableEditor::show() {
    createWindow();

    // 消息循环
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void VariableEditor::createWindow() {
    // 注册窗口类
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"VariableEditorClass";

    RegisterClassW(&wc);

    // 计算窗口尺寸
    int height = 80 + variables_.size() * 50 + 50; // 标题 + 变量 + 按钮
    int width = 400;

    // 创建窗口
    hwnd_ = CreateWindowW(
        L"VariableEditorClass", L"变量编辑器",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr
    );

    // 创建控件
    font_ = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                      L"Microsoft YaHei UI");

    createControls();

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

void VariableEditor::createControls() {
    // 创建标题
    HWND titleLabel = CreateWindowW(
        L"STATIC", L"变量编辑器",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        10, 10, 380, 25,
        hwnd_, (HMENU)-1, nullptr, nullptr
    );
    SendMessageW(titleLabel, WM_SETFONT, (WPARAM)font_, TRUE);

    // 创建分隔线
    CreateWindowW(
        L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        10, 40, 380, 1,
        hwnd_, nullptr, nullptr, nullptr
    );

    // 创建变量控件
    for (size_t i = 0; i < variables_.size(); i++) {
        auto& var = variables_[i];
        int y = 50 + i * 50;

        // 变量名标签
        std::wstring wname(var.name.begin(), var.name.end());
        var.label = CreateWindowW(
            L"STATIC", wname.c_str(),
            WS_CHILD | WS_VISIBLE,
            20, y + 10, 100, 20,
            hwnd_, nullptr, nullptr, nullptr
        );
        SendMessageW(var.label, WM_SETFONT, (WPARAM)font_, TRUE);

        // 根据类型创建不同控件
        if (var.type == InputType::SLIDER) {
            var.control = CreateWindowW(
                L"msctls_trackbar32", L"",
                WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                120, y, 180, 30,
                hwnd_, (HMENU)(1000 + i), nullptr, nullptr
            );

            // 设置滑块范围和位置
            int range = (int)((var.max - var.min) / var.step);
            SendMessageW(var.control, TBM_SETRANGE, TRUE, MAKELPARAM(0, range));
            int pos = (int)((var.tempValue - var.min) / var.step);
            SendMessageW(var.control, TBM_SETPOS, TRUE, pos);

            // 值显示标签
            std::wstringstream ss;
            ss.precision(2);
            ss << std::fixed << var.tempValue;
            HWND valueLabel = CreateWindowW(
                L"STATIC", ss.str().c_str(),
                WS_CHILD | WS_VISIBLE | SS_RIGHT,
                310, y + 5, 70, 20,
                hwnd_, (HMENU)(2000 + i), nullptr, nullptr
            );
            SendMessageW(valueLabel, WM_SETFONT, (WPARAM)font_, TRUE);
        } else {
            var.control = CreateWindowW(
                L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | ES_RIGHT,
                120, y + 5, 100, 25,
                hwnd_, (HMENU)(3000 + i), nullptr, nullptr
            );
            SendMessageW(var.control, WM_SETFONT, (WPARAM)font_, TRUE);

            // 设置初始值
            std::wstringstream ss;
            ss.precision(2);
            ss << std::fixed << var.tempValue;
            SetWindowTextW(var.control, ss.str().c_str());

            // 显示范围
            std::wstringstream rangeStr;
            rangeStr.precision(1);
            rangeStr << L"[" << std::fixed << var.min << L"-" << var.max << L"]";
            HWND rangeLabel = CreateWindowW(
                L"STATIC", rangeStr.str().c_str(),
                WS_CHILD | WS_VISIBLE,
                230, y + 5, 150, 20,
                hwnd_, nullptr, nullptr, nullptr
            );
            SendMessageW(rangeLabel, WM_SETFONT, (WPARAM)font_, TRUE);
        }
    }

    // 创建确认按钮
    int buttonY = 60 + variables_.size() * 50;
    HWND applyButton = CreateWindowW(
        L"BUTTON", L"应用修改",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, buttonY, 100, 30,
        hwnd_, (HMENU)9000, nullptr, nullptr
    );
    SendMessageW(applyButton, WM_SETFONT, (WPARAM)font_, TRUE);
}

void VariableEditor::updateControlValue(int index) {
    if (index < 0 || index >= variables_.size()) return;

    auto& var = variables_[index];

    if (var.type == InputType::SLIDER) {
        // 更新滑块值显示
        std::wstringstream ss;
        ss.precision(2);
        ss << std::fixed << var.tempValue;
        SetWindowTextW((HWND)GetDlgItem(hwnd_, 2000 + index), ss.str().c_str());
    }
}

void VariableEditor::readControlValue(int index) {
    if (index < 0 || index >= variables_.size()) return;

    auto& var = variables_[index];

    if (var.type == InputType::SLIDER) {
        // 从滑块读取值
        int pos = SendMessageW(var.control, TBM_GETPOS, 0, 0);
        var.tempValue = var.min + pos * var.step;
    } else {
        // 从编辑框读取值
        WCHAR text[32] = {0};
        GetWindowTextW(var.control, text, 32);
        try {
            float value = std::stof(text);
            // 限制范围
            if (value < var.min) value = var.min;
            if (value > var.max) value = var.max;
            var.tempValue = value;
        } catch (...) {
            // 转换失败，恢复原值
            std::wstringstream ss;
            ss.precision(2);
            ss << std::fixed << var.tempValue;
            SetWindowTextW(var.control, ss.str().c_str());
        }
    }

    updateControlValue(index);
}

void VariableEditor::applyChanges() {
    // 应用所有变量的修改
    for (auto& var : variables_) {
        *var.value = var.tempValue;
    }

    MessageBoxW(hwnd_, L"变量值已更新", L"成功", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK VariableEditor::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!instance_) return DefWindowProcW(hwnd, msg, wParam, lParam);

    switch (msg) {
        case WM_HSCROLL: {
            // 处理滑块改变
            HWND hCtrl = (HWND)lParam;
            int id = GetDlgCtrlID(hCtrl);
            if (id >= 1000 && id < 1000 + (int)instance_->variables_.size()) {
                int index = id - 1000;
                instance_->readControlValue(index);
            }
            return 0;
        }

        case WM_COMMAND: {
            int id = LOWORD(wParam);
            int notification = HIWORD(wParam);

            // 确认按钮
            if (id == 9000 && notification == BN_CLICKED) {
                instance_->applyChanges();
                return 0;
            }

            // 编辑框内容改变
            if (id >= 3000 && id < 3000 + (int)instance_->variables_.size() &&
                notification == EN_CHANGE) {
                int index = id - 3000;
                instance_->readControlValue(index);
                return 0;
            }

            break;
        }

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            instance_->hwnd_ = nullptr;
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
