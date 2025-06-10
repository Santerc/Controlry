#include "include/PIDController.h"
#include <algorithm>

PIDController::PIDController(float kp, float ki, float kd, float outputMin, float outputMax, float integral_max)
    : kp_(kp), ki_(ki), kd_(kd)
    , outputMin_(outputMin), outputMax_(outputMax)
    , integral_(0.0f), lastError_(0.0f)
    ,  integral_max_(integral_max)
{}

void PIDController::setGains(float kp, float ki, float kd) {
    kp_ = kp;
    ki_ = ki;
    kd_ = kd;
}

void PIDController::setLimits(float outputMin, float outputMax) {
    outputMin_ = outputMin;
    outputMax_ = outputMax;
}

float PIDController::compute(float setpoint, float measurement, float dt) {
    float error = setpoint - measurement;

    // 积分项
    integral_ += ki_ * error * dt;
    integral_ = std::clamp(integral_, -integral_max_, integral_max_);

    // 微分项
    float derivative = (error - lastError_) / dt;
    lastError_ = error;

    // 计算输出
    float output = kp_ * error + integral_ + kd_ * derivative;

    // 限幅
    return std::clamp(output, outputMin_, outputMax_);
}

void PIDController::reset() {
    integral_ = 0.0f;
    lastError_ = 0.0f;
}