#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <chrono>
#include <cfloat>

class DebugInterface {
public:
    enum class InputType {
        SLIDER,
        DRAG,
        INPUT_BOX
    };

    enum class ViewMode {
        WAVEFORM,    // 波形显示
        NUMERIC      // 数值显示
    };

    struct WaveformConfig {
        float timeWindow = 1.0f;      // 时间窗口（秒，支持毫秒级）
        float refreshRate = 60.0f;    // 刷新率（Hz）
        bool autoScale = true;        // 自动缩放
        float minY = -100.0f;         // Y轴最小值（手动缩放时）
        float maxY = 100.0f;          // Y轴最大值（手动缩放时）
        bool showGrid = true;         // 显示网格
        bool showCursor = true;       // 显示游标
        float zoomFactor = 1.0f;      // 缩放因子
    };

    DebugInterface();
    ~DebugInterface();

    // 编辑器功能 - 添加可编辑变量
    void addEditableVariable(const std::string& name, float* value,
                            float min = 0.0f, float max = 100.0f, float step = 1.0f,
                            InputType type = InputType::SLIDER);

    // 查看器功能 - 添加要监控的变量
    void addWatchVariable(const std::string& name, float* value,
                         ViewMode mode = ViewMode::WAVEFORM,
                         const std::string& unit = "",
                         unsigned int color = 0xFF00FF00);

    // 移除变量
    void removeEditableVariable(const std::string& name);
    void removeWatchVariable(const std::string& name);

    // 设置波形配置
    void setWaveformConfig(const WaveformConfig& config);

    // 数据采集控制
    void startCapture();
    void stopCapture();
    void pauseCapture();
    bool isCapturing() const;
    void clearHistory();

    // 设置主题
    void setTheme(int themeId);

    // 显示调试界面
    void show();

private:
    // 编辑器相关结构
    struct EditableVariable {
        std::string name;
        float* value;
        float tempValue;
        float min;
        float max;
        float step;
        InputType type;
    };

    // 查看器相关结构
    struct DataPoint {
        float timestamp;
        float value;
    };

    struct WatchVariable {
        std::string name;
        float* valuePtr;
        ViewMode mode;
        std::string unit;
        unsigned int color;
        std::deque<DataPoint> history;
        float lastValue;
        float minValue;
        float maxValue;
        bool visible = true;
    };

    // 渲染函数
    void renderImGui();
    void renderEditorPanel();
    void renderViewerPanel();
    void renderWaveformArea();
    void renderWatchArea();
    void renderControlBar();

    // 编辑器功能
    void applyChanges();

    // 查看器功能
    void updateData();
    void drawWaveform(const WatchVariable& var, const std::vector<float>& times,
                     const std::vector<float>& values, unsigned int color);
    void drawGrid();
    void drawCursor();

    // 工具函数
    float getCurrentTime() const;
    void applyMacOSStyle();
    unsigned int getDefaultColor(int index);

    // 数据成员
    std::vector<EditableVariable> editableVariables_;
    std::vector<WatchVariable> watchVariables_;
    WaveformConfig waveformConfig_;

    // 状态变量
    bool showWindow_;
    bool isCapturing_;
    bool isPaused_;
    bool changesApplied_;
    int themeId_;

    // 时间相关
    std::chrono::steady_clock::time_point startTime_;
    std::chrono::steady_clock::time_point lastUpdateTime_;

    // UI状态
    float cursorTime_;
    bool showCursorValue_;
    int selectedVariable_;

    // 布局控制
    float editorHeight_;
    float viewerHeight_;
    float controlBarHeight_;
    bool showEditor_;
    bool showViewer_;

    float pausedTimeStart_ = 0.0f;
    float pausedTimeEnd_ = 0.0f;

};