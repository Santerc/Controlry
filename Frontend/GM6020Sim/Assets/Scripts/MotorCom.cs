using UnityEngine;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System;
using System.Linq;
using System.IO.Ports;
using System.Collections.Generic;

public enum ComMethod{
    Tcp,
    Uart,
}

public class MotorComServer : MonoBehaviour
{
    [Header("Communication Settings")]
    public ComMethod communicationMethod = ComMethod.Tcp;
    
    [Header("TCP Settings")]
    public int listenPort = 6000;
    
    [Header("UART Settings")]
    public string serialPortName = "COM1";
    public int baudRate = 115200;
    public Parity parity = Parity.None;
    public int dataBits = 8;
    public StopBits stopBits = StopBits.One;
    
    [Header("Motor Settings")]
    public byte motorId = 1; // 默认电机ID
    
    [Header("General Settings")]
    public float reconnectDelay = 2f;
    
    // 通信常量
    private const byte FEEDBACK_HEADER = 0xA0;
    private const byte COMMAND_HEADER = 0xA1;
    private const int FEEDBACK_PACKET_SIZE = 11; // A0 + ID + Angle(4) + Speed(4) + Checksum
    private const int COMMAND_PACKET_SIZE = 7;   // A1 + ID + Torque(4) + Checksum
    
    // TCP相关
    private TcpListener listener;
    private TcpClient client;
    private NetworkStream stream;
    
    // UART相关（本来想做485的结果没钱买了QAQ）
    private SerialPort serialPort;
    
    // 通用
    private Thread commThread;
    private volatile bool running = false;
    private volatile bool shouldRun = true;
    private byte[] receiveBuffer = new byte[1024];
    private int bufferPosition = 0;

    public MotorSim motorSim;
    
    public bool IsShouldRun()
    {
        return shouldRun;
    }

    void Start()
    {
        if (motorSim == null)
        {
            Debug.LogError("MotorSim reference not set!");
            return;
        }

        StartCommunication();
    }

    void StartCommunication()
    {
        switch (communicationMethod)
        {
            case ComMethod.Tcp:
                StartTcpServer();
                break;
            case ComMethod.Uart:
                StartRS485();
                break;
        }
    }

    #region TCP Communication
    void StartTcpServer()
    {
        try
        {
            // 先清理可能存在的监听器
            if (listener != null)
            {
                try
                {
                    listener.Stop();
                }
                catch { }
                listener = null;
            }

            listener = new TcpListener(IPAddress.Any, listenPort);
            
            // 设置Socket选项允许地址重用
            listener.Server.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
            
            listener.Start();
            Debug.Log($"✓ TCP Server started on port {listenPort}, waiting for client...");
            
            // 开始监听客户端连接
            BeginAcceptClient();
        }
        catch (Exception e)
        {
            Debug.LogError($"✗ Failed to start TCP server: {e.Message}");
            
            // 检查端口是否被占用，如果是则尝试其他端口
            if (e.Message.Contains("套接字地址") || e.Message.Contains("address already in use"))
            {
                Debug.LogWarning($"Port {listenPort} is in use. Trying alternative ports...");
                TryAlternativePort();
            }
            else
            {
                // 其他错误，稍后重试
                Invoke("StartTcpServer", reconnectDelay);
            }
        }
    }

    void BeginAcceptClient()
    {
        try
        {
            if (listener != null && shouldRun)
            {
                listener.BeginAcceptTcpClient(OnClientAccepted, null);
                Debug.Log("Listening for new client connections...");
            }
        }
        catch (Exception e)
        {
            Debug.LogError($"Error starting accept operation: {e.Message}");
            if (shouldRun)
            {
                // 如果出错，稍后重试
                Invoke("BeginAcceptClient", 1f);
            }
        }
    }

    void OnClientAccepted(IAsyncResult ar)
    {
        TcpClient newClient = null;
        
        try
        {
            // 检查listener是否仍然有效
            if (listener == null || !shouldRun)
            {
                Debug.Log("Server is shutting down, ignoring client connection");
                return;
            }

            newClient = listener.EndAcceptTcpClient(ar);
            
            if (newClient == null)
            {
                Debug.LogWarning("Failed to accept client - client is null");
                return;
            }

            // 如果已经有客户端连接，断开旧连接
            if (client != null)
            {
                Debug.Log("Disconnecting previous client for new connection");
                CleanupTcpConnection();
                Thread.Sleep(100); // 短暂等待清理完成
            }

            client = newClient;
            stream = client.GetStream();
            running = true;
            
            Debug.Log($"✓ TCP Client connected from: {client.Client.RemoteEndPoint}");

            // 启动通信线程
            commThread = new Thread(TcpCommLoop);
            commThread.IsBackground = true;
            commThread.Start();
        }
        catch (ObjectDisposedException)
        {
            // Listener已被释放，这是正常的关闭流程
            Debug.Log("Listener disposed - server shutting down");
            return;
        }
        catch (Exception e)
        {
            Debug.LogError($"Error accepting TCP client: {e.Message}");
            
            // 清理失败的连接
            if (newClient != null)
            {
                try { newClient.Close(); } catch { }
            }
            CleanupTcpConnection();
        }
        finally
        {
            // 重要：继续监听新的连接
            if (shouldRun && listener != null)
            {
                Invoke("BeginAcceptClient", 0.1f); // 稍微延迟以避免立即重试
            }
        }
    }

    void TcpCommLoop()
    {
        Debug.Log("TCP Communication started");

        while (running && shouldRun)
        {
            try
            {
                if (client == null || !client.Connected || stream == null)
                {
                    Debug.LogWarning("TCP client connection lost");
                    break;
                }

                // 读取数据
                int bytesRead = stream.Read(receiveBuffer, bufferPosition, receiveBuffer.Length - bufferPosition);
                if (bytesRead == 0)
                {
                    throw new Exception("TCP Client disconnected - no data received");
                }
                
                bufferPosition += bytesRead;
                ProcessReceivedData();
                
                // 发送反馈数据
                SendFeedbackPacket();
                
                Thread.Sleep(1);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"TCP Communication error: {e.Message}");
                break;
            }
        }

        Debug.Log("TCP Communication ended");
        CleanupTcpConnection();
        
        if (shouldRun && listener != null)
        {
            Invoke("BeginAcceptClient", 1f);
        }
    }

    void CleanupTcpConnection()
    {
        running = false;
        
        try
        {
            if (stream != null)
            {
                stream.Close();
                stream.Dispose();
                stream = null;
            }
        }
        catch (Exception e)
        {
            Debug.LogWarning($"Error closing stream: {e.Message}");
        }

        try
        {
            if (client != null)
            {
                if (client.Connected)
                    client.Close();
                client = null;
            }
        }
        catch (Exception e)
        {
            Debug.LogWarning($"Error closing TCP client: {e.Message}");
        }

        Debug.Log("TCP client connection cleaned up");
    }

    void StopTcpServer()
    {
        try
        {
            shouldRun = false; // 停止接受新连接
            
            if (listener != null)
            {
                listener.Stop();
                listener = null;
                Debug.Log("TCP Server stopped");
            }
        }
        catch (Exception e)
        {
            Debug.LogWarning($"Error stopping TCP server: {e.Message}");
        }
    }

    void TryAlternativePort()
    {
        int originalPort = listenPort;
        for (int i = 1; i <= 10; i++)
        {
            int testPort = originalPort + i;
            if (IsPortAvailable(testPort))
            {
                Debug.Log($"Found available port: {testPort}");
                listenPort = testPort;
                Invoke("StartTcpServer", 0.1f);
                return;
            }
        }
        
        Debug.LogError($"No available ports found near {originalPort}. Please check what's using port {originalPort}");
        listenPort = originalPort; // 恢复原端口号
        
        // 稍后重试原端口
        Invoke("StartTcpServer", reconnectDelay);
    }

    bool IsPortAvailable(int port)
    {
        try
        {
            TcpListener testListener = new TcpListener(IPAddress.Any, port);
            testListener.Start();
            testListener.Stop();
            return true;
        }
        catch
        {
            return false;
        }
    }
    #endregion

    #region RS485 Communication
    void StartRS485()
    {
        StopTcpServer();
        try
        {
            // 先清理可能存在的串口连接
            if (serialPort != null && serialPort.IsOpen)
            {
                try
                {
                    serialPort.Close();
                    serialPort.Dispose();
                }
                catch { }
                serialPort = null;
            }

            serialPort = new SerialPort(serialPortName, baudRate, parity, dataBits, stopBits);
            serialPort.ReadTimeout = 1000;
            serialPort.WriteTimeout = 1000;
            serialPort.Open();
            
            running = true;
            shouldRun = true;
            Debug.Log($"✓ RS485 connection opened on {serialPortName} at {baudRate} baud");

            commThread = new Thread(RS485CommLoop);
            commThread.IsBackground = true;
            commThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError($"✗ Failed to start RS485: {e.Message}");
            
            if (shouldRun)
            {
                Debug.Log($"Will retry RS485 connection in {reconnectDelay} seconds...");
                Invoke("StartRS485", reconnectDelay);
            }
        }
    }

    void RS485CommLoop()
    {
        Debug.Log("RS485 Communication started");

        while (running && shouldRun)
        {
            try
            {
                if (serialPort == null || !serialPort.IsOpen)
                {
                    Debug.LogWarning("RS485 serial port closed");
                    break;
                }

                // 读取数据
                int bytesToRead = serialPort.BytesToRead;
                if (bytesToRead > 0)
                {
                    int bytesRead = serialPort.Read(receiveBuffer, bufferPosition, 
                        Math.Min(bytesToRead, receiveBuffer.Length - bufferPosition));
                    bufferPosition += bytesRead;
                    ProcessReceivedData();
                }

                // 发送反馈数据
                SendFeedbackPacket();
                
                Thread.Sleep(1);
            }
            catch (TimeoutException)
            {
                // 读取超时是正常的，继续循环
                continue;
            }
            catch (Exception e)
            {
                Debug.LogWarning($"RS485 Communication error: {e.Message}");
                break;
            }
        }

        Debug.Log("RS485 Communication ended");
        CleanupRS485Connection();
        
        if (shouldRun)
        {
            Invoke("StartRS485", reconnectDelay);
        }
    }

    void CleanupRS485Connection()
    {
        running = false;
        try
        {
            if (serialPort != null)
            {
                if (serialPort.IsOpen)
                    serialPort.Close();
                serialPort.Dispose();
                serialPort = null;
            }
            Debug.Log("RS485 connection cleaned up");
        }
        catch (Exception e)
        {
            Debug.LogError($"Error during RS485 cleanup: {e.Message}");
        }
    }
    #endregion
    
    #region Packet Processing
    void ProcessReceivedData()
    {
        while (bufferPosition >= COMMAND_PACKET_SIZE)
        {
            // 查找命令包头
            int packetStart = -1;
            for (int i = 0; i <= bufferPosition - COMMAND_PACKET_SIZE; i++)
            {
                if (receiveBuffer[i] == COMMAND_HEADER)
                {
                    packetStart = i;
                    break;
                }
            }

            if (packetStart == -1)
            {
                // 没有找到完整包，清理缓冲区
                if (bufferPosition > COMMAND_PACKET_SIZE)
                {
                    Array.Copy(receiveBuffer, bufferPosition - COMMAND_PACKET_SIZE + 1, 
                             receiveBuffer, 0, COMMAND_PACKET_SIZE - 1);
                    bufferPosition = COMMAND_PACKET_SIZE - 1;
                }
                return;
            }

            // 检查是否有足够数据
            if (bufferPosition - packetStart < COMMAND_PACKET_SIZE)
            {
                // 数据不足，等待更多数据
                if (packetStart > 0)
                {
                    Array.Copy(receiveBuffer, packetStart, receiveBuffer, 0, bufferPosition - packetStart);
                    bufferPosition -= packetStart;
                }
                return;
            }

            // 校验和检查
            byte checksum = 0;
            for (int i = 0; i < COMMAND_PACKET_SIZE - 1; i++)
            {
                checksum ^= receiveBuffer[packetStart + i];
            }

            if (checksum != receiveBuffer[packetStart + COMMAND_PACKET_SIZE - 1])
            {
                Debug.LogWarning($"Checksum error in command packet at position {packetStart}");
                bufferPosition--;
                Array.Copy(receiveBuffer, packetStart + 1, receiveBuffer, packetStart, bufferPosition - packetStart);
                continue;
            }

            // 提取电机ID和扭矩值
            byte receivedId = receiveBuffer[packetStart + 1];
            if (receivedId == motorId) // 只处理本电机ID的数据
            {
                float torque = BitConverter.ToSingle(receiveBuffer, packetStart + 2);
                motorSim.motors[motorId].torqueInput = torque;
                
                Debug.Log($"Received torque command for motor {receivedId}: {torque:F3} Nm");
            }

            // 移除已处理的数据包
            int remainingBytes = bufferPosition - (packetStart + COMMAND_PACKET_SIZE);
            if (remainingBytes > 0)
            {
                Array.Copy(receiveBuffer, packetStart + COMMAND_PACKET_SIZE, 
                         receiveBuffer, 0, remainingBytes);
            }
            bufferPosition = remainingBytes;
        }
    }

    void SendFeedbackPacket()
    {
        if ((communicationMethod == ComMethod.Tcp && (client == null || !client.Connected)) ||
            (communicationMethod == ComMethod.Uart && (serialPort == null || !serialPort.IsOpen)))
        {
            return;
        }

        try
        {
            byte[] packet = new byte[FEEDBACK_PACKET_SIZE];
            
            // 包头
            packet[0] = FEEDBACK_HEADER;
            // 电机ID
            packet[1] = motorId;
            // 角度数据（转换为度）
            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.motors[motorId].angle * Mathf.Rad2Deg), 0, packet, 2, 4);
            // 速度数据
            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.motors[motorId].angularVelocity), 0, packet, 6, 4);
            
            // 计算校验和
            byte checksum = 0;
            for (int i = 0; i < FEEDBACK_PACKET_SIZE - 1; i++)
            {
                checksum ^= packet[i];
            }
            packet[FEEDBACK_PACKET_SIZE - 1] = checksum;

            // 发送数据
            if (communicationMethod == ComMethod.Tcp)
            {
                stream.Write(packet, 0, packet.Length);
            }
            else
            {
                serialPort.Write(packet, 0, packet.Length);
            }
        }
        catch (Exception ex)
        {
            Debug.LogWarning($"Error sending feedback packet: {ex.Message}");
        }
    }
    #endregion
    
    #region Common Methods
    void ProcessTorqueData(byte[] torqueBuf)
    {
        // 打印原始字节数据
        string bytesHex = BitConverter.ToString(torqueBuf);
        string bytesInt = string.Join(", ", torqueBuf.Select(b => ((int)b).ToString()));
        Debug.Log($"收到的原始字节(Hex): [{bytesHex}]");
        Debug.Log($"收到的原始字节(Int): [{bytesInt}]");
        
        // 打印转换后的力矩值
        float torqueValue = BitConverter.ToSingle(torqueBuf, 0);
        Debug.Log($"转换后的力矩值: {torqueValue:F3} Nm");

        // 更新MotorSim的输入力矩
        motorSim.motors[motorId].torqueInput = Mathf.Clamp(torqueValue, -1.8f, 1.8f);
        Debug.Log($"Motor Torque Input: {motorSim.motors[motorId].torqueInput}");
    }

    void CleanupAll()
    {
        Debug.Log("=== Starting cleanup ===");
        shouldRun = false;
        running = false;

        // 停止所有Invoke调用
        CancelInvoke();

        // 等待通信线程结束
        if (commThread != null && commThread.IsAlive)
        {
            Debug.Log("Waiting for communication thread to finish...");
            if (!commThread.Join(2000)) // 等待最多2秒
            {
                Debug.LogWarning("Communication thread did not finish gracefully, aborting...");
                try
                {
                    commThread.Abort();
                }
                catch (Exception e)
                {
                    Debug.LogWarning($"Error aborting thread: {e.Message}");
                }
            }
            commThread = null;
        }

        switch (communicationMethod)
        {
            case ComMethod.Tcp:
                CleanupTcpConnection();
                StopTcpServer();
                break;
            case ComMethod.Uart:
                CleanupRS485Connection();
                break;
        }

        Debug.Log("=== Cleanup completed ===");
    }

    // 添加一些调试方法
    [ContextMenu("Check Connection Status")]
    public void CheckConnectionStatus()
    {
        string status = "=== CONNECTION STATUS ===\n";
        status += $"Communication Method: {communicationMethod}\n";
        status += $"Should Run: {shouldRun}\n";
        status += $"Running: {running}\n";
        
        if (communicationMethod == ComMethod.Tcp)
        {
            status += $"TCP Listener: {(listener != null ? "Active" : "Null")}\n";
            status += $"TCP Client: {(client != null && client.Connected ? "Connected" : "Disconnected")}\n";
            status += $"Current Port: {listenPort}\n";
            
            if (client != null && client.Connected)
            {
                try
                {
                    status += $"Client Endpoint: {client.Client.RemoteEndPoint}\n";
                }
                catch
                {
                    status += "Client Endpoint: Unknown\n";
                }
            }
        }
        else
        {
            status += $"Serial Port: {(serialPort != null && serialPort.IsOpen ? "Open" : "Closed")}\n";
            status += $"Port Name: {serialPortName}\n";
            status += $"Baud Rate: {baudRate}\n";
        }
        
        status += $"Communication Thread: {(commThread != null && commThread.IsAlive ? "Active" : "Inactive")}\n";
        status += "========================";
        
        Debug.Log(status);
    }

    [ContextMenu("Restart Communication")]
    public void RestartCommunication()
    {
        Debug.Log("=== Manually restarting communication ===");
        CleanupAll();
        shouldRun = true;
        Invoke("StartCommunication", 0.5f);
    }
    #endregion

    void OnDestroy()
    {
        CleanupAll();
    }
    
    #region Public Properties for UI
    /// <summary>
    /// 获取服务器是否应该运行的状态
    /// </summary>
    public bool ShouldRun => shouldRun;
    
    /// <summary>
    /// 获取当前是否正在运行通信
    /// </summary>
    public bool IsRunning => running;
    #endregion

    #region Public Methods for UI
    /// <summary>
    /// 检查TCP客户端是否已连接
    /// </summary>
    /// <returns>如果TCP客户端已连接返回true</returns>
    public bool IsClientConnected()
    {
        return client != null && client.Connected && running;
    }

    /// <summary>
    /// 检查RS485串口是否已打开
    /// </summary>
    /// <returns>如果串口已打开返回true</returns>
    public bool IsSerialPortOpen()
    {
        return serialPort != null && serialPort.IsOpen && running;
    }

    /// <summary>
    /// 获取当前连接状态
    /// </summary>
    /// <returns>如果当前通信方式已连接返回true</returns>
    public bool IsConnected()
    {
        switch (communicationMethod)
        {
            case ComMethod.Tcp:
                return IsClientConnected();
            case ComMethod.Uart:
                return IsSerialPortOpen();
            default:
                return false;
        }
    }

    /// <summary>
    /// 获取连接信息字符串
    /// </summary>
    /// <returns>当前连接的详细信息</returns>
    public string GetConnectionInfo()
    {
        switch (communicationMethod)
        {
            case ComMethod.Tcp:
                if (IsClientConnected())
                {
                    try
                    {
                        return $"TCP Client: {client.Client.RemoteEndPoint}";
                    }
                    catch
                    {
                        return "TCP Client: Connected";
                    }
                }
                return $"TCP Server: Listening on port {listenPort}";
                
            case ComMethod.Uart:
                if (IsSerialPortOpen())
                {
                    return $"RS485: {serialPortName} @ {baudRate} baud";
                }
                return $"RS485: Disconnected ({serialPortName})";
                
            default:
                return "Unknown method";
        }
    }
    #endregion

    // 在Inspector中切换通信方式时重启通信
    void OnValidate()
    {
        if (Application.isPlaying && shouldRun)
        {
            CleanupAll();
            shouldRun = true;
            StartCommunication();
        }
    }
}