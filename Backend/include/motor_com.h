#ifndef MOTOR_COM_H
#define MOTOR_COM_H

#include <winsock2.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

class Motor;

class MotorCommunication {
public:
    explicit MotorCommunication(Motor* motor);
    ~MotorCommunication();

    bool connect(const std::string& ipAddress, int port);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

private:
    SOCKET sock;
    bool connected;
    std::atomic<bool> shouldExit;

    Motor* motor;
    
    // Threads
    std::thread senderThread;
    std::thread receiverThread;

    std::mutex consoleMutex;

    void sendThreadFunc();
    void receiveThreadFunc();
    
    // Winsock
    static bool initializeWinsock();
};

#endif // MOTOR_COM_H