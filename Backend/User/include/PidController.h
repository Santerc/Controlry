#ifndef PID_CONTROLLER_H
#define PID_CONTROLLER_H

class PIDController {
public:
    PIDController(float kp, float ki, float kd, float outputMin, float outputMax,  float integral_max);

    // 设置PID参数
    void setGains(float kp, float ki, float kd);
    void setLimits(float outputMin, float outputMax);

    // 计算PID输出
    float compute(float setpoint, float measurement, float dt);

    // 重置控制器
    void reset();

    float kp_;
    float ki_;
    float kd_;
    float outputMin_;
    float outputMax_;
    float integral_;
    float integral_max_;
    float lastError_;

private:
};

#endif // PID_CONTROLLER_H