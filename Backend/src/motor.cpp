#include "motor.h"
#include "motor_com.h"
#include <iostream>

Motor::Motor(int motorId) : 
    motorId(motorId),
    torqueToSend(0.0f),
    currentAngle(0.0f),
    currentOmega(0.0f) {
    // Create the communication object
    communication = std::make_unique<MotorCommunication>(this);
}

Motor::~Motor() {
    disconnect();
}

bool Motor::connect(const std::string& ipAddress, int port) {
    return communication->connect(ipAddress, port);
}

void Motor::disconnect() {
    if (communication) {
        communication->disconnect();
    }
}

bool Motor::isConnected() const {
    return communication && communication->isConnected();
}

void Motor::setTorque(float torqueNm) {
    torqueToSend.store(torqueNm);
}

float Motor::getTorque() const {
    return torqueToSend.load();
}

float Motor::getCurrentAngle() const {
    return currentAngle.load();
}

float Motor::getCurrentOmega() const {
    return currentOmega.load();
}

int Motor::getMotorId() const {
    return motorId;
}