#include "motor_com.h"
#include "motor.h"
#include <iostream>
#include <cstring>
#include <chrono>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

// 通信协议常量
const uint8_t FEEDBACK_HEADER = 0xA0;
const uint8_t COMMAND_HEADER = 0xA1;
const size_t FEEDBACK_PACKET_SIZE = 11; // A0 + ID + Angle(4) + Speed(4) + Checksum
const size_t COMMAND_PACKET_SIZE = 7;   // A1 + ID + Torque(4) + Checksum

MotorCommunication::MotorCommunication(Motor* motor) :
    sock(INVALID_SOCKET),
    connected(false),
    shouldExit(false),
    motor(motor),
    receiveBuffer(256),
    bufferPosition(0) {
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
        return true;
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

    if (::connect(sock, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        closesocket(sock);
        sock = INVALID_SOCKET;
        return false;
    }

    std::cout << "Connected to Unity server at " << ipAddress << ":" << port
              << " for motor ID " << static_cast<int>(motor->getMotorId()) << std::endl;
    connected = true;
    shouldExit = false;

    // Start threads
    senderThread = std::thread(&MotorCommunication::sendThreadFunc, this);
    receiverThread = std::thread(&MotorCommunication::receiveThreadFunc, this);

    return true;
}

void MotorCommunication::disconnect() {
    if (!connected) {
        return;
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
    std::cout << "Disconnected motor ID " << static_cast<int>(motor->getMotorId()) << " from server." << std::endl;
}

bool MotorCommunication::isConnected() const {
    return connected;
}

void MotorCommunication::sendThreadFunc() {
    while (!shouldExit) {
        // 准备控制指令包 (上位机发送扭矩指令)
        std::vector<uint8_t> packet(COMMAND_PACKET_SIZE);

        // 包头
        packet[0] = COMMAND_HEADER;
        // 电机ID
        packet[1] = motor->getMotorId();

        // 扭矩数据
        float torque = motor->getTorque();
        std::memcpy(&packet[2], &torque, 4);

        // 计算校验和
        uint8_t checksum = 0;
        for (size_t i = 0; i < COMMAND_PACKET_SIZE - 1; ++i) {
            checksum ^= packet[i];
        }
        packet[COMMAND_PACKET_SIZE - 1] = checksum;

        // 发送控制指令
        int sent = send(sock, reinterpret_cast<const char*>(packet.data()), packet.size(), 0);
        if (sent != COMMAND_PACKET_SIZE) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "Motor ID " << static_cast<int>(motor->getMotorId())
                      << " - Failed to send command packet." << std::endl;
            shouldExit = true;
            break;
        }

        // 调试输出
        {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cout << "Motor ID " << static_cast<int>(motor->getMotorId())
                      << " - Sent torque command: " << torque << " Nm" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void MotorCommunication::receiveThreadFunc() {
    while (!shouldExit) {
        // 接收反馈数据
        int received = recv(sock, reinterpret_cast<char*>(receiveBuffer.data() + bufferPosition),
                     receiveBuffer.size() - bufferPosition, 0);
        if (received <= 0) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "Motor ID " << static_cast<int>(motor->getMotorId())
                      << " - Receive failed or disconnected." << std::endl;
            shouldExit = true;
            return;
        }

        bufferPosition += received;
        processReceivedData();
    }
}

void MotorCommunication::processReceivedData() {
    while (bufferPosition >= FEEDBACK_PACKET_SIZE) {
        // 查找反馈包头
        size_t packetStart = 0;
        bool found = false;

        for (size_t i = 0; i <= bufferPosition - FEEDBACK_PACKET_SIZE; ++i) {
            if (receiveBuffer[i] == FEEDBACK_HEADER) {
                packetStart = i;
                found = true;
                break;
            }
        }

        if (!found) {
            // 没有找到完整包，清理缓冲区
            if (bufferPosition > FEEDBACK_PACKET_SIZE) {
                std::memmove(receiveBuffer.data(), receiveBuffer.data() + bufferPosition - FEEDBACK_PACKET_SIZE + 1,
                            FEEDBACK_PACKET_SIZE - 1);
                bufferPosition = FEEDBACK_PACKET_SIZE - 1;
            }
            return;
        }

        // 检查是否有足够数据
        if (bufferPosition - packetStart < FEEDBACK_PACKET_SIZE) {
            // 数据不足，等待更多数据
            if (packetStart > 0) {
                std::memmove(receiveBuffer.data(), receiveBuffer.data() + packetStart, bufferPosition - packetStart);
                bufferPosition -= packetStart;
            }
            return;
        }

        // 校验和检查
        uint8_t checksum = 0;
        for (size_t i = 0; i < FEEDBACK_PACKET_SIZE - 1; ++i) {
            checksum ^= receiveBuffer[packetStart + i];
        }

        if (checksum != receiveBuffer[packetStart + FEEDBACK_PACKET_SIZE - 1]) {
            std::lock_guard<std::mutex> lock(consoleMutex);
            std::cerr << "Motor ID " << static_cast<int>(motor->getMotorId())
                      << " - Checksum error in feedback packet." << std::endl;

            bufferPosition--;
            std::memmove(receiveBuffer.data() + packetStart, receiveBuffer.data() + packetStart + 1,
                        bufferPosition - packetStart);
            continue;
        }

        // 提取电机ID和反馈数据
        uint8_t receivedId = receiveBuffer[packetStart + 1];
        if (receivedId == motor->getMotorId()) {
            float angleDeg, omega;
            std::memcpy(&angleDeg, &receiveBuffer[packetStart + 2], 4);
            std::memcpy(&omega, &receiveBuffer[packetStart + 6], 4);

            // 更新电机状态
            motor->currentAngle = angleDeg;
            motor->currentOmega = omega;

            // // 调试输出
            // {
            //     std::lock_guard<std::mutex> lock(consoleMutex);
            //     std::cout << "Motor ID " << static_cast<int>(motor->getMotorId())
            //               << " - Received feedback: Angle=" << angleDeg
            //               << "°, Omega=" << omega << "rad/s" << std::endl;
            // }
        }

        // 移除已处理的数据包
        size_t remainingBytes = bufferPosition - (packetStart + FEEDBACK_PACKET_SIZE);
        if (remainingBytes > 0) {
            std::memmove(receiveBuffer.data(), receiveBuffer.data() + packetStart + FEEDBACK_PACKET_SIZE,
                        remainingBytes);
        }
        bufferPosition = remainingBytes;
    }
}