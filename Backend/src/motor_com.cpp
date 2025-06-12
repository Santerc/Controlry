#include "motor_com.h"
#include "motor.h"
#include <iostream>
#include <cstring>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

MotorCommunication::MotorCommunication(Motor* motor) : 
    sock(INVALID_SOCKET),
    connected(false),
    shouldExit(false),
    motor(motor) {
    initializeWinsock();
}

MotorCommunication::~MotorCommunication() {
    disconnect();
}

bool MotorCommunication::initializeWinsock() {
    static bool initialized = false;
    
    if (!initialized) {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed." << std::endl;
            return false;
        }
        initialized = true;
    }
    
    return true;
}

bool MotorCommunication::connect(const std::string& ipAddress, int port) {
    if (connected) {
        return true; // Already connected
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    serverAddr.sin_addr.s_addr = inet_addr(ipAddress.c_str());

    if (::connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    std::cout << "Connected to Unity server at " << ipAddress << ":" << port 
              << " for motor ID " << motor->getMotorId() << std::endl;
    connected = true;
    shouldExit = false;

    // Start threads
    senderThread = std::thread(&MotorCommunication::sendThreadFunc, this);
    receiverThread = std::thread(&MotorCommunication::receiveThreadFunc, this);

    return true;
}

void MotorCommunication::disconnect() {
    if (!connected) {
        return; // Already disconnected
    }

    shouldExit = true;
    
    if (senderThread.joinable()) {
        senderThread.join();
    }
    
    if (receiverThread.joinable()) {
        receiverThread.join();
    }
    
    closesocket(sock);
    sock = INVALID_SOCKET;
    connected = false;
    std::cout << "Disconnected motor ID " << motor->getMotorId() << " from server." << std::endl;
}

bool MotorCommunication::isConnected() const {
    return connected;
}

void MotorCommunication::sendThreadFunc() {
    while (!shouldExit) {
        float torque = motor->getTorque();
        char torqueBuffer[4];
        std::memcpy(torqueBuffer, &torque, 4);

        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            // std::cout << "Motor ID " << motor->getMotorId() << " - Sending torque bytes: ";
            // for(int i = 0; i < 4; i++) {
            //     std::cout << static_cast<int>(static_cast<unsigned char>(torqueBuffer[i])) << " ";
            // }

            // Create a temporary buffer to reconstruct
            float reconstructed;
            std::memcpy(&reconstructed, torqueBuffer, 4);
            // std::cout << "-> Reconstructed value: " << reconstructed << " Nm" << std::endl;
        }

        int sent = send(sock, torqueBuffer, 4, 0);
        if (sent != 4) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "Motor ID " << motor->getMotorId() << " - Send failed." << std::endl;
            shouldExit = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MotorCommunication::receiveThreadFunc() {
    while (!shouldExit) {
        char recvBuffer[8];
        int received = 0;
        while (received < 8) {
            int ret = recv(sock, recvBuffer + received, 8 - received, 0);
            if (ret <= 0) {
                std::lock_guard<std::mutex> lock(consoleMutex);
                std::cerr << "Motor ID " << motor->getMotorId() 
                          << " - Receive failed or disconnected." << std::endl;
                shouldExit = true;
                return;
            }
            received += ret;
        }

        float angleDeg, omega;
        std::memcpy(&angleDeg, recvBuffer, 4);
        std::memcpy(&omega, recvBuffer + 4, 4);

        motor->currentAngle.store(angleDeg);
        motor->currentOmega.store(omega);

        // Optional debug output
        /*{
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Motor ID " << motor->getMotorId() 
                      << " - Angle: " << angleDeg << " deg, Omega: " << omega << " rad/s" << std::endl;
        }*/
    }
}