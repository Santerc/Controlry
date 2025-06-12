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
    // Constructor and destructor
    explicit MotorCommunication(Motor* motor);
    ~MotorCommunication();

    // Connection methods
    bool connect(const std::string& ipAddress, int port);
    void disconnect();
    [[nodiscard]] bool isConnected() const;

private:
    // Socket and connection
    SOCKET sock;
    bool connected;
    std::atomic<bool> shouldExit;
    
    // Associated motor
    Motor* motor;
    
    // Threads
    std::thread senderThread;
    std::thread receiverThread;
    
    // Thread synchronization
    std::mutex consoleMutex;
    
    // Thread functions
    void sendThreadFunc();
    void receiveThreadFunc();
    
    // Winsock initialization
    static bool initializeWinsock();
};

#endif // MOTOR_COM_H