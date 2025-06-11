#include "include/MotorControl.h"
#include <chrono>
#include <cmath>
#include "motor_manager.h"

#include "include/PidController.h"

// 全局变量定义
std::atomic<bool> g_running{false};
std::atomic<float> g_targetTorque{1.5f};

float omega_watch = 0.0f;  // 用于监控角度反馈

PIDController SpeedController(0.4f, 0.02f, 0.0f, -10.0f, 10.0f, 1.0f);

void torqueUpdateLoop() {
    using clock = std::chrono::high_resolution_clock;
    using duration = std::chrono::duration<double, std::milli>;

    const duration target_duration(1.0);  // 1ms周期
    auto next_time = clock::now();
    double time = 0.0;

    while (g_running) {
        // 更新电机转矩
        auto& motorManager = MotorManager::getInstance();
        if (Motor* motor = motorManager.getMotor(0)) {
            omega_watch = motor->getCurrentOmega();
            float torque = SpeedController.compute(
                5.F,
                motor->getCurrentOmega(),
                0.001f  // dt = 1ms
            );
            motor->setTorque(torque);
            time += 0.001;
        }

        // 精确等待到下一个时间点
        next_time += std::chrono::duration_cast<clock::duration>(target_duration);
        std::this_thread::sleep_until(next_time);
    }
}

void startTorqueControl(std::thread& controlThread) {
    if (!g_running) {
        g_running = true;
        controlThread = std::thread(torqueUpdateLoop);
    }
}

void stopTorqueControl(std::thread& controlThread) {
    g_running = false;
    if (controlThread.joinable()) {
        controlThread.join();
    }
}