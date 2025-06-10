#pragma once

#include <string>
#include <vector>
#include <map>
#include <windows.h>

class VariableEditor {
public:
    enum class InputType {
        SLIDER,
        TEXT_INPUT
    };

    VariableEditor();
    ~VariableEditor();

    // 添加要编辑的变量
    void addVariable(const std::string& name, float* value,
                    float min = 0.0f, float max = 100.0f, float step = 1.0f,
                    InputType type = InputType::SLIDER);

    // 显示编辑器窗口 (阻塞方式)
    void show();

private:
    struct Variable {
        std::string name;
        float* value;
        float tempValue;
        float min;
        float max;
        float step;
        InputType type;
        HWND control;
        HWND label;
    };

    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void createWindow();
    void createControls();
    void updateControlValue(int index);
    void readControlValue(int index);
    void applyChanges();

    static VariableEditor* instance_;
    HWND hwnd_;
    std::vector<Variable> variables_;
    HFONT font_;
};
