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
    public float reconnectDelay = 1f; // 重连延迟时间(秒)
    
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
            listener = new TcpListener(IPAddress.Any, listenPort);
            listener.Start();
            Debug.Log($"TCP Server started on port {listenPort}, waiting for client...");
            listener.BeginAcceptTcpClient(OnClientAccepted, null);
        }
        catch (Exception e)
        {
            Debug.LogError($"Failed to start TCP server: {e.Message}");
            Invoke("StartTcpServer", reconnectDelay);
        }
    }

    void OnClientAccepted(IAsyncResult ar)
    {
        try
        {
            client = listener.EndAcceptTcpClient(ar);
            stream = client.GetStream();
            running = true;
            Debug.Log("TCP Client connected.");

            commThread = new Thread(TcpCommLoop);
            commThread.IsBackground = true;
            commThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError($"Error accepting TCP client: {e.Message}");
            CleanupTcpConnection();
        }
        finally
        {
            // 继续监听新的连接
            if (shouldRun && listener != null)
            {
                listener.BeginAcceptTcpClient(OnClientAccepted, null);
            }
        }
    }

    void TcpCommLoop()
    {
        byte[] torqueBuf = new byte[4];
        byte[] feedbackBuf = new byte[8];

        while (running)
        {
            try
            {
                // 读取力矩
                int readCount = 0;
                while (readCount < 4)
                {
                    int r = stream.Read(torqueBuf, readCount, 4 - readCount);
                    if (r == 0)
                    {
                        throw new Exception("TCP Client disconnected");
                    }
                    readCount += r;
                }

                ProcessTorqueData(torqueBuf);

                try
                {
                    // 发送反馈
                    Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Angle * Mathf.Rad2Deg), 0, feedbackBuf, 0, 4);
                    Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Speed), 0, feedbackBuf, 4, 4);
                    stream.Write(feedbackBuf, 0, 8);
                }
                catch (Exception)
                {
                    // 即使发送失败也继续运行
                    continue;
                }

                Thread.Sleep(10);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"TCP Communication error: {e.Message}");
                CleanupTcpConnection();
                break;
            }
        }
    }

    void CleanupTcpConnection()
    {
        running = false;
        try
        {
            stream?.Close();
            client?.Close();
        }
        catch (Exception e)
        {
            Debug.LogError($"Error during TCP cleanup: {e.Message}");
        }
    }
    #endregion

    #region RS485 Communication
    void StartRS485()
    {
        try
        {
            serialPort = new SerialPort(serialPortName, baudRate, parity, dataBits, stopBits);
            serialPort.ReadTimeout = 1000;
            serialPort.WriteTimeout = 1000;
            serialPort.Open();
            
            running = true;
            Debug.Log($"RS485 connection opened on {serialPortName} at {baudRate} baud");

            commThread = new Thread(RS485CommLoop);
            commThread.IsBackground = true;
            commThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError($"Failed to start RS485: {e.Message}");
            Invoke("StartRS485", reconnectDelay);
        }
    }

    void RS485CommLoop()
    {
        byte[] torqueBuf = new byte[4];
        byte[] feedbackBuf = new byte[8];

        while (running)
        {
            try
            {
                // RS485读取力矩数据
                int readCount = 0;
                while (readCount < 4 && running)
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
                        Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Angle * Mathf.Rad2Deg), 0, feedbackBuf, 0, 4);
                        Buffer.BlockCopy(BitConverter.GetBytes(motorSim.Speed), 0, feedbackBuf, 4, 4);
                        serialPort.Write(feedbackBuf, 0, 8);
                    }
                    catch (Exception ex)
                    {
                        Debug.LogWarning($"RS485 write error: {ex.Message}");
                        // 发送失败也继续运行
                    }
                }

                Thread.Sleep(10);
            }
            catch (Exception e)
            {
                Debug.LogWarning($"RS485 Communication error: {e.Message}");
                CleanupRS485Connection();
                // 尝试重新连接
                if (shouldRun)
                {
                    Thread.Sleep((int)(reconnectDelay * 1000));
                    StartRS485();
                }
                break;
            }
        }
    }

    void CleanupRS485Connection()
    {
        running = false;
        try
        {
            if (serialPort != null && serialPort.IsOpen)
            {
                serialPort.Close();
                serialPort.Dispose();
                serialPort = null;
            }
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
        shouldRun = false;
        running = false;

        switch (communicationMethod)
        {
            case ComMethod.Tcp:
                CleanupTcpConnection();
                try
                {
                    listener?.Stop();
                }
                catch (Exception e)
                {
                    Debug.LogError($"Error stopping TCP listener: {e.Message}");
                }
                break;
            case ComMethod.Rs485:
                CleanupRS485Connection();
                break;
        }

        if (commThread != null && commThread.IsAlive)
        {
            commThread.Join(1000); // 等待最多1秒
            if (commThread.IsAlive)
            {
                Debug.LogWarning("Force abort communication thread");
                commThread.Abort();
            }
        }
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