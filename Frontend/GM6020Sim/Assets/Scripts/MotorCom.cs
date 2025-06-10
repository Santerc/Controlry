using UnityEngine;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System;
using System.Linq; // 添加这行

public class MotorComServer : MonoBehaviour
{
    public int listenPort = 6000;
    public float reconnectDelay = 1f; // 重连延迟时间(秒)
    
    private TcpListener listener;
    private TcpClient client;
    private NetworkStream stream;
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

        StartServer();
    }

    void StartServer()
    {
        try
        {
            listener = new TcpListener(IPAddress.Any, listenPort);
            listener.Start();
            Debug.Log("Server started, waiting for client...");
            listener.BeginAcceptTcpClient(OnClientAccepted, null);
        }
        catch (Exception e)
        {
            Debug.LogError($"Failed to start server: {e.Message}");
            Invoke("StartServer", reconnectDelay);
        }
    }

    void OnClientAccepted(IAsyncResult ar)
    {
        try
        {
            client = listener.EndAcceptTcpClient(ar);
            stream = client.GetStream();
            running = true;
            Debug.Log("Client connected.");

            commThread = new Thread(CommLoop);
            commThread.IsBackground = true;
            commThread.Start();
        }
        catch (Exception e)
        {
            Debug.LogError($"Error accepting client: {e.Message}");
            CleanupConnection();
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

    void CommLoop()
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
                        throw new Exception("Client disconnected");
                    }
                    readCount += r;
                }


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
                Debug.Log(motorSim.TorqueInput);

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
                Debug.LogWarning($"Communication error: {e.Message}");
                CleanupConnection();
                break;
            }
        }
    }

    void CleanupConnection()
    {
        running = false;
        try
        {
            stream?.Close();
            client?.Close();
        }
        catch (Exception e)
        {
            Debug.LogError($"Error during cleanup: {e.Message}");
        }
    }

    void OnDestroy()
    {
        shouldRun = false;
        running = false;
        CleanupConnection();
        try
        {
            listener?.Stop();
        }
        catch (Exception e)
        {
            Debug.LogError($"Error stopping listener: {e.Message}");
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
}