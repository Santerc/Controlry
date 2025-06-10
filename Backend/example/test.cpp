#include "include/ui.h"
#include <iostream>

float speed = 50.0f;
float volume = 75.0f;
float brightness = 100.0f;
float customValue = 42.0f;

int uitest() {
    // 创建需要编辑的变量


    // 创建编辑器
    VariableEditor editor;

    // 设置主题 (0=深色, 1=浅色, 2=经典)
    editor.setTheme(0);  // 深色主题

    // 添加变量
    editor.addVariable("1", &speed, 0.0f, 100.0f, 1.0f, VariableEditor::InputType::SLIDER);
    editor.addVariable("2", &volume, 0.0f, 100.0f, 1.0f, VariableEditor::InputType::SLIDER);
    editor.addVariable("3", &brightness, 0.0f, 100.0f, 1.0f, VariableEditor::InputType::SLIDER);
    editor.addVariable("Value", &customValue, -100.0f, 100.0f, 0.1f, VariableEditor::InputType::DRAG);

    // 显示编辑器 (阻塞直到窗口关闭)
    editor.show();

    // 编辑器关闭后，变量可能已被更新
    std::cout << "1: " << speed << std::endl;
    std::cout << "2: " << volume << std::endl;
    std::cout << "3: " << brightness << std::endl;
    std::cout << "Value: " << customValue << std::endl;

    return 0;
}
