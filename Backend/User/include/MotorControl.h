#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <atomic>
#include <thread>
#include "include/PidController.h"

// 全局控制标志
extern std::atomic<bool> g_running;
extern std::atomic<float> g_targetTorque;
extern PIDController SpeedController;
extern float omega_watch;  // 用于监控角度反馈

// 转矩控制线程函数
void torqueUpdateLoop();

// 启动和停止转矩控制的函数
void startTorqueControl(std::thread& controlThread);
void stopTorqueControl(std::thread& controlThread);

#endif // MOTOR_CONTROL_H