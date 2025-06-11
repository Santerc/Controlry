#include <iostream>
#include "motor_manager.h"
#include "include/MotorControl.h"
#include "include/debug.h"

int main() {
    MotorManager& motorManager = MotorManager::getInstance();

    // 创建多个电机
    const int numMotors = 1;
    const int basePort = 6000;

    for (int i = 0; i < numMotors; i++) {
        motorManager.createMotor(i);
        std::cout << "Created motor with ID " << i << std::endl;

        if (!motorManager.connectMotor(i, "127.0.0.1", basePort + i)) {
            std::cerr << "Failed to connect motor " << i << " on port " << (basePort + i) << std::endl;
            return 1;
        }

        Motor* motor = motorManager.getMotor(i);
        if (motor) {
            motor->setTorque(1.5f + i * 0.5f);
            std::cout << "Motor ID " << i << " connected on port " << (basePort + i)
                     << ". Initial torque set to " << motor->getTorque() << " Nm." << std::endl;
        }
    }

    std::thread controlThread;
    startTorqueControl(controlThread);
    std::cout << "All motors connected. Press Enter to exit..." << std::endl;
    startDebugThread();

    std::cin.get();

    stopDebugThread();
    stopTorqueControl(controlThread);
    motorManager.disconnectAll();

    return 0;
}