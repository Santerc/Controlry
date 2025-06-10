#include <iostream>
#include <cstring>
#include <thread>
#include <chrono>
#include <winsock2.h>
#include <mutex>
#include <atomic>

#pragma comment(lib, "ws2_32.lib")

// 共享数据结构
struct SharedData {
    std::atomic<float> torqueToSend{1.5f};
    std::atomic<float> currentAngle{0.0f};
    std::atomic<float> currentOmega{0.0f};
    std::atomic<bool> shouldExit{false};
    std::mutex consoleMutex;
};

// 发送线程函数
void sendThread(SOCKET sock, SharedData& data) {
    while (!data.shouldExit) {
        float torque = data.torqueToSend.load();
        char torqueBuffer[4];
        std::memcpy(torqueBuffer, &torque, 4);

        {
            std::lock_guard<std::mutex> lock(data.consoleMutex);
            std::cout << "Sending torque: " << torque << " Nm" << std::endl;
        }

        int sent = send(sock, torqueBuffer, 4, 0);
        if (sent != 4) {
            std::lock_guard<std::mutex> lock(data.consoleMutex);
            std::cerr << "Send failed." << std::endl;
            data.shouldExit = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// 接收线程函数
void receiveThread(SOCKET sock, SharedData& data) {
    while (!data.shouldExit) {
        char recvBuffer[8];
        int received = 0;
        while (received < 8) {
            int ret = recv(sock, recvBuffer + received, 8 - received, 0);
            if (ret <= 0) {
                std::lock_guard<std::mutex> lock(data.consoleMutex);
                std::cerr << "Receive failed or disconnected." << std::endl;
                data.shouldExit = true;
                return;
            }
            received += ret;
        }

        float angleDeg, omega;
        std::memcpy(&angleDeg, recvBuffer, 4);
        std::memcpy(&omega, recvBuffer + 4, 4);

        data.currentAngle.store(angleDeg);
        data.currentOmega.store(omega);

        // 可选的调试输出
        /*{
            std::lock_guard<std::mutex> lock(data.consoleMutex);
            std::cout << "Angle: " << angleDeg << " deg, Omega: " << omega << " rad/s" << std::endl;
        }*/
    }
}

int main() {
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(6000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to Unity server." << std::endl;

    // 创建共享数据
    SharedData sharedData;

    // 创建发送和接收线程
    std::thread sender(sendThread, sock, std::ref(sharedData));
    std::thread receiver(receiveThread, sock, std::ref(sharedData));

    // 等待用户输入来退出
    std::cout << "Press Enter to exit..." << std::endl;
    std::cin.get();

    // 清理
    sharedData.shouldExit = true;
    sender.join();
    receiver.join();
    closesocket(sock);
    WSACleanup();

    return 0;
}