using UnityEngine;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System;
using System.Linq;
using System.IO.Ports;

public enum ComMethod{
    Tcp,
    Rs485,
}

public class MotorComServer : MonoBehaviour
{
    [Header("Communication Settings")]
    public ComMethod communicationMethod = ComMethod.Tcp;
    
    [Header("TCP Settings")]
    public int listenPort = 6000;
    
    [Header("RS485 Settings")]
    public string serialPortName = "COM1";
    public int baudRate = 115200;
    public Parity parity = Parity.None;
    public int dataBits = 8;
    public StopBits stopBits = StopBits.One;
    
    [Header("General Settings")]
    public float reconnectDelay = 2f; // 重连延迟时间(秒)
    
    // TCP相关
    private TcpListener listener;
    private TcpClient client;
    private NetworkStream stream;
    
    // RS485相关
    private SerialPort serialPort;
    
    // 通用
    private Thread commThread;
    private volatile bool running = false;
    private volatile bool shouldRun = true; // 控制服务运行状态

    public MotorSim motorSim;

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
            case ComMethod.Rs485:
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
        byte[] torqueBuf = new byte[4];
        byte[] feedbackBuf = new byte[8];

        Debug.Log("TCP Communication started");

        while (running && shouldRun)
        {
            try
            {
                // 检查连接是否仍然有效
                if (client == null || !client.Connected || stream == null)
                {
                    Debug.LogWarning("TCP client connection lost");
                    break;
                }

                // 读取力矩数据
                int readCount = 0;
                while (readCount < 4 && running)
                {
                    if (!client.Connected)
                    {
                        throw new Exception("TCP Client disconnected during read");
                    }

                    int r = stream.Read(torqueBuf, readCount, 4 - readCount);
                    if (r == 0)
                    {
                        throw new Exception("TCP Client disconnected - no data received");
                    }
                    readCount += r;
                }

                if (readCount == 4)
                {
                    ProcessTorqueData(torqueBuf);

                    try
                    {
                        // 发送反馈数据
                        if (client != null && client.Connected && stream != null)
                        {
                            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Angle * Mathf.Rad2Deg), 0, feedbackBuf, 0, 4);
                            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Speed), 0, feedbackBuf, 4, 4);
                            stream.Write(feedbackBuf, 0, 8);
                            stream.Flush();
                        }
                    }
                    catch (Exception ex)
                    {
                        Debug.LogWarning($"TCP send error: {ex.Message}");
                        break; // 发送失败，退出循环
                    }
                }

                Thread.Sleep(10);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"TCP Communication error: {e.Message}");
                break;
            }
        }

        Debug.Log("TCP Communication ended");
        
        // 通信结束，清理连接并准备接受新连接
        CleanupTcpConnection();
        
        // 如果服务器还在运行，准备接受新连接
        if (shouldRun && listener != null)
        {
            Debug.Log("Preparing to accept new connections...");
            Invoke("BeginAcceptClient", 1f); // 1秒后重新开始监听
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
        byte[] torqueBuf = new byte[4];
        byte[] feedbackBuf = new byte[8];

        Debug.Log("RS485 Communication started");

        while (running && shouldRun)
        {
            try
            {
                // 检查串口是否仍然打开
                if (serialPort == null || !serialPort.IsOpen)
                {
                    Debug.LogWarning("RS485 serial port closed");
                    break;
                }

                // RS485读取力矩数据
                int readCount = 0;
                while (readCount < 4 && running && serialPort.IsOpen)
                {
                    try
                    {
                        int bytesToRead = Math.Min(serialPort.BytesToRead, 4 - readCount);
                        if (bytesToRead > 0)
                        {
                            int r = serialPort.Read(torqueBuf, readCount, bytesToRead);
                            readCount += r;
                        }
                        else
                        {
                            Thread.Sleep(1); // 短暂等待避免过度占用CPU
                        }
                    }
                    catch (TimeoutException)
                    {
                        // 读取超时，继续尝试
                        continue;
                    }
                }

                if (readCount == 4)
                {
                    ProcessTorqueData(torqueBuf);

                    try
                    {
                        // 发送反馈数据
                        if (serialPort != null && serialPort.IsOpen)
                        {
                            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Angle * Mathf.Rad2Deg), 0, feedbackBuf, 0, 4);
                            Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Speed), 0, feedbackBuf, 4, 4);
                            serialPort.Write(feedbackBuf, 0, 8);
                        }
                    }
                    catch (Exception ex)
                    {
                        Debug.LogWarning($"RS485 write error: {ex.Message}");
                        break; // 写入失败，退出循环
                    }
                }

                Thread.Sleep(10);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"RS485 Communication error: {e.Message}");
                break;
            }
        }

        Debug.Log("RS485 Communication ended");
        
        // 通信结束，清理连接
        CleanupRS485Connection();
        
        // 如果服务器还在运行，尝试重新连接
        if (shouldRun)
        {
            Debug.Log($"Will attempt RS485 reconnection in {reconnectDelay} seconds...");
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
        motorSim.TorqueInput = torqueValue;
        Debug.Log($"Motor Torque Input: {motorSim.TorqueInput}");
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
            case ComMethod.Rs485:
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