#pragma once

#include <string>
#include <vector>
#include <functional>

class VariableEditor {
public:
    enum class InputType {
        SLIDER,
        DRAG,
        INPUT_BOX
    };

    VariableEditor();
    ~VariableEditor();

    // 添加要编辑的变量
    void addVariable(const std::string& name, float* value,
                    float min = 0.0f, float max = 100.0f, float step = 1.0f,
                    InputType type = InputType::SLIDER);

    // 设置主题颜色 (0=深色, 1=浅色, 2=经典)
    void setTheme(int themeId);

    // 显示编辑器窗口
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
    };

    void setupImGui();
    void renderImGui();
    void applyChanges();
    void cleanupImGui();

    std::vector<Variable> variables_;
    bool showWindow_;
    bool changesApplied_;
    int themeId_;
};
