#ifndef DEBUG_H
#define DEBUG_H

#include <thread>
#include <atomic>
#include <string>
#include <vector>

// 状态声明
extern std::thread debugThread;
extern std::atomic<bool> debugThreadRunning;

// 线程控制函数
void startDebugThread();
void stopDebugThread();

#endif //DEBUG_H