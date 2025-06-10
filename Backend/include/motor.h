#ifndef MOTOR_H
#define MOTOR_H

#include <atomic>
#include <memory>
#include <string>

// Forward declaration
class MotorCommunication;

class Motor {
public:
    // Constructor and destructor
    Motor(int motorId = 0);
    ~Motor();

    // Connection methods
    bool connect(const std::string& ipAddress = "127.0.0.1", int port = 6000);
    void disconnect();
    bool isConnected() const;

    // Control methods
    void setTorque(float torqueNm);
    float getTorque() const;
    
    // Status methods
    float getCurrentAngle() const;
    float getCurrentOmega() const;
    int getMotorId() const;

private:
    // Motor ID
    int motorId;
    
    // Communication
    std::unique_ptr<MotorCommunication> communication;
    
    // Data
    std::atomic<float> torqueToSend;
    std::atomic<float> currentAngle;
    std::atomic<float> currentOmega;
    
    // Friend class to allow MotorCommunication to update motor state
    friend class MotorCommunication;
};

#endif // MOTOR_H