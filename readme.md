<center><h1>Controlry</h1></center>

# 概述

Controlry 是一个为 RoboMaster (RM) 电控培训项目设计的教学工具。它同时支持物理单片机 UART 控制和 Windows 仿真控制方法。

本项目通过以下方式实现电控系统的实践：
- 通过 UART 通信进行硬件控制
- Windows 环境下的软件仿真( *TCP* 通信)

# 通信格式

以下是电机通信协议：

## 1. 数据包类型
协议定义了两种数据包：
- 反馈数据包 (Feedback Packet)
- 命令数据包 (Command Packet)

## 2. 反馈数据包 (11字节)
电机 → 控制器的状态反馈

| 字节位置 | 长度 | 说明 | 取值 |
|---------|------|------|------|
| 0 | 1字节 | 包头 | 0xA0 |
| 1 | 1字节 | 电机ID | 0~255 |
| 2-5 | 4字节 | 角度 | float (度) |
| 6-9 | 4字节 | 角速度 | float (rad/s) |
| 10 | 1字节 | 校验和 | XOR(0~9) |

## 3. 命令数据包 (7字节)
控制器 → 电机的控制命令

| 字节位置 | 长度 | 说明 | 取值 |
|---------|------|------|------|
| 0 | 1字节 | 包头 | 0xA1 |
| 1 | 1字节 | 电机ID | 0~255 |
| 2-5 | 4字节 | 力矩 | float (Nm) |
| 6 | 1字节 | 校验和 | XOR(0~5) |

## 4. 校验和计算
- 校验方式：异或校验（XOR）
- 计算范围：从包头到数据末尾（不含校验和字节）
- 校验和位置：数据包最后一字节

# TCP通信C++框架介绍
[点击跳转](./Backend/readme.md)

# 注意！
一定要先开 `6020.exe`再运行控制端！

# Author

<img src="https://avatars.githubusercontent.com/u/131346045?s=96&v=4" width="32" height="32" style="vertical-align:middle;border-radius:50%;" />：[Santerc](https://github.com/Santerc)
北理自动化专业大三学生 RoboMaster追梦战队退休菜鸡电控
