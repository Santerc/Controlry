#ifndef MOTOR_H
#define MOTOR_H

#include <atomic>
#include <memory>
#include <string>

class MotorCommunication;

class Motor {
public:
    Motor(int motorId = 0);
    ~Motor();

    bool connect(const std::string& ipAddress = "127.0.0.1", int port = 6000);
    void disconnect();
    bool isConnected() const;

    void setTorque(float torqueNm);
    float getTorque() const;

    float getCurrentAngle() const;
    float getCurrentOmega() const;
    int getMotorId() const;

private:
    int motorId;

    std::unique_ptr<MotorCommunication> communication;

    std::atomic<float> torqueToSend;
    std::atomic<float> currentAngle;
    std::atomic<float> currentOmega;

    friend class MotorCommunication;
};

#endif // MOTOR_H