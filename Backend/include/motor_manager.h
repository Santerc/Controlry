#ifndef MOTOR_MANAGER_H
#define MOTOR_MANAGER_H

#include <map>
#include <memory>
#include <string>
#include "motor.h"

class MotorManager {
public:
    // Singleton访问
    static MotorManager& getInstance();

    // 电机创建和访问
    Motor* createMotor(int motorId);
    Motor* getMotor(int motorId);
    void removeMotor(int motorId);

    // 连接单个电机到指定服务器和端口
    bool connectMotor(int motorId, const std::string& ipAddress = "127.0.0.1", int port = 6000);

    // 连接所有电机（使用递增端口）
    bool connectAll(const std::string& ipAddress = "127.0.0.1", int basePort = 6000);

    // 断开所有电机
    void disconnectAll();

private:
    MotorManager() = default;
    ~MotorManager();

    MotorManager(const MotorManager&) = delete;
    MotorManager& operator=(const MotorManager&) = delete;
    MotorManager(MotorManager&&) = delete;
    MotorManager& operator=(MotorManager&&) = delete;

    std::map<int, std::unique_ptr<Motor>> motors;
};

#endif // MOTOR_MANAGER_H