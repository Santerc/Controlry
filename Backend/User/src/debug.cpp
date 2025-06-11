#include "include/debug.h"
#include "include/ui.h"
#include "motor_manager.h"
#include "include/MotorControl.h"
#include "imgui.h"

#include <thread>
#include <atomic>

std::thread debugThread;
std::atomic<bool> debugThreadRunning{false};

DebugInterface debugInterface;

void Debug_init() {
    // 创建整合的调试界面


    // ========== 添加可编辑变量 ==========
    debugInterface.addEditableVariable("kp", &SpeedController.kp_, -500.0f, 500.0f, 0.01f,
                                       DebugInterface::InputType::INPUT_BOX);
    debugInterface.addEditableVariable("ki", &SpeedController.ki_, -500.0f, 500.0f, 0.01f,
                                       DebugInterface::InputType::INPUT_BOX);
    debugInterface.addEditableVariable("kd", &SpeedController.kd_, -500.0f, 500.0f, 0.01f,
                                       DebugInterface::InputType::INPUT_BOX);

    // ========== 添加波形监控变量 ==========
    debugInterface.addWatchVariable("角度反馈", &omega_watch,
                                    DebugInterface::ViewMode::WAVEFORM, "rad/s",
                                    IM_COL32(0, 120, 250, 255));    // 蓝色

    // ========== 添加数值监控变量 ==========
    debugInterface.addWatchVariable("角度反馈", &omega_watch,
                                    DebugInterface::ViewMode::NUMERIC, "提取方法…");

    // ========== 配置波形显示 ==========
    DebugInterface::WaveformConfig config;
    config.timeWindow = 2.0f;
    config.refreshRate = 60.0f;
    config.autoScale = true;         // 自动缩放
    config.showGrid = true;          // 显示网格
    config.showCursor = true;        // 显示游标
    config.zoomFactor = 1.0f;        // 初始缩放
    debugInterface.setWaveformConfig(config);
}

void Debug_update() {
    // 启动数据采集
    debugInterface.startCapture();

    // 显示调试界面
    debugInterface.show();
}

void debugThreadFunction() {
    Debug_init();

    while (debugThreadRunning) {
        Debug_update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps
    }
}

// 启动调试线程
void startDebugThread() {
    if (!debugThreadRunning) {
        debugThreadRunning = true;
        debugThread = std::thread(debugThreadFunction);
    }
}

// 停止调试线程
void stopDebugThread() {
    if (debugThreadRunning) {
        debugThreadRunning = false;
        if (debugThread.joinable()) {
            debugThread.join();
        }
    }
}


