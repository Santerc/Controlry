#include "motor_manager.h"
#include <iostream>

MotorManager& MotorManager::getInstance() {
    static MotorManager instance;
    return instance;
}

Motor* MotorManager::createMotor(int motorId) {
    auto result = motors.emplace(motorId, std::make_unique<Motor>(motorId));
    return result.first->second.get();
}

Motor* MotorManager::getMotor(int motorId) {
    auto it = motors.find(motorId);
    return it != motors.end() ? it->second.get() : nullptr;
}

void MotorManager::removeMotor(int motorId) {
    motors.erase(motorId);
}

bool MotorManager::connectMotor(int motorId, const std::string& ipAddress, int port) {
    Motor* motor = getMotor(motorId);
    if (!motor) {
        std::cerr << "Motor " << motorId << " not found." << std::endl;
        return false;
    }
    return motor->connect(ipAddress, port);
}

bool MotorManager::connectAll(const std::string& ipAddress, int basePort) {
    bool allConnected = true;
    for (const auto& [motorId, motor] : motors) {
        if (!connectMotor(motorId, ipAddress, basePort + motorId)) {
            std::cerr << "Failed to connect motor " << motorId
                      << " on port " << (basePort + motorId) << std::endl;
            allConnected = false;
        } else {
            std::cout << "Motor " << motorId << " connected on port "
                      << (basePort + motorId) << std::endl;
        }
    }
    return allConnected;
}

void MotorManager::disconnectAll() {
    for (const auto& [motorId, motor] : motors) {
        motor->disconnect();
    }
}

MotorManager::~MotorManager() {
    disconnectAll();
}