using UnityEngine;
using TMPro;
using UnityEngine.UI;
using System.IO.Ports;
using System.Collections.Generic;

public class MotorUI : MonoBehaviour
{
    [Header("Motor Reference")]
    public MotorSim motorSim;
    public MotorComServer motorComServer;

    [Header("Motor Status UI")]
    public TextMeshProUGUI angleText;
    public TextMeshProUGUI speedText;
    public TextMeshProUGUI torqueText;
    
    [Header("Connection Status UI")]
    public Image connectionStatusLight; // 连接状态指示灯（圆形Image）
    public TextMeshProUGUI connectionStatusText; // 连接状态文字
    public TextMeshProUGUI communicationMethodText; // 通信方式显示
    public TextMeshProUGUI portInfoText; // 端口/串口信息
    public TextMeshProUGUI detailedInfoText; // 详细连接信息

    [Header("Status Light Settings")]
    [SerializeField] private float breathingSpeed = 2f; // 呼吸灯频率
    [SerializeField] private float minAlpha = 0.3f; // 最小透明度
    [SerializeField] private float maxAlpha = 1f; // 最大透明度
    
    [Header("RS485 Settings")]
    public TMP_Dropdown comPortDropdown;
    
    // UI颜色配置
    private Color baseColor = new Color(1f, 1f, 1f, 0.9f); // 白色略透明
    private Color highlightColor = new Color(1f, 0.55f, 0f, 1f); // 大疆橙色
    private Color connectedColor = new Color(0f, 0.8f, 0f, 1f); // 绿色
    private Color disconnectedColor = new Color(0.9f, 0f, 0f, 1f); // 红色
    private Color waitingColor = new Color(1f, 0.8f, 0f, 1f); // 黄色（等待连接）

    void Start()
    {
        // 初始化时检查引用
        if (motorComServer == null)
        {
            motorComServer = FindFirstObjectByType<MotorComServer>();
            if (motorComServer == null)
            {
                Debug.LogWarning("MotorComServer not found! Connection status will not be displayed.");
            }
        }
        InitializeComPorts();
    }
    
    void InitializeComPorts()
    {
        if (comPortDropdown == null) return;

        // 清除现有选项
        comPortDropdown.ClearOptions();

        // 获取所有可用的串口
        string[] ports = SerialPort.GetPortNames();
        List<string> portOptions = new List<string>(ports);

        // 如果没有找到串口，添加一个提示选项
        if (portOptions.Count == 0)
        {
            portOptions.Add("No COM ports");
        }

        // 更新下拉框选项
        comPortDropdown.AddOptions(portOptions);

        // 设置当前选中的串口
        int currentIndex = portOptions.IndexOf(motorComServer.serialPortName);
        if (currentIndex >= 0)
        {
            comPortDropdown.value = currentIndex;
        }

        // 添加选择改变事件监听
        comPortDropdown.onValueChanged.AddListener(OnComPortChanged);

        // 根据当前通信方式显示/隐藏串口设置
        comPortDropdown.transform.parent.gameObject.SetActive(
            motorComServer.communicationMethod == ComMethod.Uart);
    }
    
    private void OnComPortChanged(int index)
    {
        if (motorComServer == null || index < 0 || 
            index >= comPortDropdown.options.Count) return;

        string selectedPort = comPortDropdown.options[index].text;
        if (selectedPort == "No COM ports") return;

        // 更新 MotorComServer 的串口设置
        motorComServer.serialPortName = selectedPort;

        // 如果当前是 RS485 模式，重启通信
        if (motorComServer.communicationMethod == ComMethod.Uart)
        {
            motorComServer.RestartCommunication();
        }
    }

    void Update()
    {
        UpdateMotorStatus();
        UpdateConnectionStatus();
        // OnToggleCommunicationMethod();
    }

    void UpdateMotorStatus()
    {
        if (motorSim == null) return;

        // 更新电机状态显示
        if (angleText != null)
            angleText.text = $"Angle: {motorSim.motors[0].angle * Mathf.Rad2Deg:F2}°";
        
        if (speedText != null)
            speedText.text = $"Omega: {motorSim.motors[0].angularVelocity:F2} rad/s";
        
        if (torqueText != null)
        {
            torqueText.text = $"Torque: {motorSim.motors[0].torqueInput:F3} Nm";
            
            // 让转矩数字周期性渐变颜色
            float t = (Mathf.Sin(Time.time * 3f) + 1f) / 2f;
            torqueText.color = Color.Lerp(baseColor, highlightColor, t);
        }
    }

    void UpdateConnectionStatus()
    {
        if (motorComServer == null) return;

        // 获取连接状态
        bool isConnected = motorComServer.IsConnected();
        bool isListening = motorComServer.IsShouldRun(); // 使用ShouldRun来判断是否在监听
        
        // 更新连接状态指示灯（呼吸灯效果）
        UpdateStatusLight(isConnected, isListening);
        
        // 更新连接状态文字
        UpdateStatusText(isConnected, isListening);
        
        // 更新通信方式显示
        UpdateCommunicationInfo();
        
        // 更新详细连接信息
        UpdateDetailedInfo();
    }

    void UpdateStatusLight(bool isConnected, bool isListening)
    {
        if (connectionStatusLight == null) return;

        Color targetColor;
        
        if (isConnected)
        {
            // 已连接 - 绿色
            targetColor = connectedColor;
        }
        else if (isListening)
        {
            // 等待连接 - 黄色
            targetColor = disconnectedColor;
        }
        else
        {
            // 断开连接 - 红色
            targetColor = disconnectedColor;
        }

        // 呼吸灯动画效果
        float breathingAlpha = Mathf.Lerp(minAlpha, maxAlpha, 
            (Mathf.Sin(Time.time * breathingSpeed * 2) + 1f) / 2f);
        targetColor.a = breathingAlpha;
        
        connectionStatusLight.color = targetColor;
    }

    void UpdateStatusText(bool isConnected, bool isListening)
    {
        if (connectionStatusText == null) return;

        string statusText;
        Color textColor;

        if (isConnected)
        {
            statusText = "Connected";
            textColor = connectedColor;
        }
        else if (isListening)
        {
            statusText = "Waiting...";
            textColor = waitingColor;
        }
        else
        {
            statusText = "Disconnected";
            textColor = disconnectedColor;
        }

        connectionStatusText.text = statusText;
        connectionStatusText.color = textColor;
    }

    void UpdateCommunicationInfo()
    {
        if (communicationMethodText == null) return;

        string methodText = $"{motorComServer.communicationMethod}";
        communicationMethodText.text = methodText;
        
        if (comPortDropdown != null)
        {
            comPortDropdown.transform.parent.gameObject.SetActive(
                motorComServer.communicationMethod == ComMethod.Uart);
        }
        
        // 根据通信方式设置不同的颜色
        switch (motorComServer.communicationMethod)
        {
            case ComMethod.Tcp:
                communicationMethodText.color = new Color(1f, 0.5f, 0.5f, 1f); // 橙色
                break;
            case ComMethod.Uart:
                communicationMethodText.color = new Color(0f, 1f, 0f, 1f); // 紫色
                break;
        }
    }
    
    public void RefreshComPorts()
    {
        InitializeComPorts();
    }

    void UpdatePortInfo()
    {
        if (portInfoText == null) return;

        string portInfo = "";
        
        switch (motorComServer.communicationMethod)
        {
            case ComMethod.Tcp:
                portInfo = $"TCP Port: {motorComServer.listenPort}";
                break;
                
            case ComMethod.Uart:
                portInfo = $"Serial: {motorComServer.serialPortName} | {motorComServer.baudRate} baud";
                break;
        }

        portInfoText.text = portInfo;
        portInfoText.gameObject.SetActive(true);
    }

    void UpdateDetailedInfo()
    {
        if (detailedInfoText == null) return;

        string detailedInfo = motorComServer.GetConnectionInfo();
        detailedInfoText.text = detailedInfo;
        
        // 根据连接状态设置颜色
        if (motorComServer.IsConnected())
        {
            detailedInfoText.color = connectedColor;
        }
        else
        {
            detailedInfoText.color = new Color(0.7f, 0.7f, 0.7f, 1f); // 灰色
        }
    }

    // 可选：添加手动刷新连接的按钮方法
    public void OnRefreshConnectionClicked()
    {
        if (motorComServer != null)
        {
            motorComServer.RestartCommunication();
        }
    }

    // 可选：添加切换通信方式的方法
    public void OnToggleCommunicationMethod()
    {
        if (motorComServer != null)
        {
            // 切换通信方式
            if (motorComServer.communicationMethod == ComMethod.Tcp)
            {
                motorComServer.communicationMethod = ComMethod.Uart;
            }
            else
            {
                motorComServer.communicationMethod = ComMethod.Tcp;
            }
            
            // 重启通信
            motorComServer.RestartCommunication();
        }
    }

    // 调试用：打印连接状态
    [ContextMenu("Print Connection Status")]
    public void PrintConnectionStatus()
    {
        if (motorComServer != null)
        {
            Debug.Log($"=== UI Connection Status ===");
            Debug.Log($"Is Connected: {motorComServer.IsConnected()}");
            Debug.Log($"Should Run: {motorComServer.IsShouldRun()}");
            Debug.Log($"Connection Info: {motorComServer.GetConnectionInfo()}");
            Debug.Log($"Communication Method: {motorComServer.communicationMethod}");
            motorComServer.CheckConnectionStatus();
        }
    }
}