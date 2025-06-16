using UnityEngine;
using UnityEngine.InputSystem;
using System;

[System.Serializable]
public class Motor
{
    [Header("Motor Components")]
    public Transform rotor;   // 转子模型
    
    [Header("Control Input")]
    public float torqueInput = 0f;
    
    [Header("Mechanical Parameters")]
    public float inertia = 0.01f;     // 转动惯量
    public float damping = 0.002f;    // 粘性摩擦系数
    public float staticFriction = 0.05f; // 静摩擦力矩
    public float loadTorque = 0.02f;  // 恒定负载力矩
    
    [Header("Runtime State")]
    public float angularVelocity = 0f; // rad/s
    public float angle = 0f;           // rad
    public byte motorId = 0;           // 电机ID
    
    public Motor(byte id = 0)
    {
        motorId = id;
        // 设置默认参数
        inertia = 0.01f;
        damping = 0.002f;
        staticFriction = 0.05f;
        loadTorque = 0.02f;
        angularVelocity = 0f;
        angle = 0f;
        torqueInput = 0f;
        // rotor = transform.Find($"RotorPivot_{motorId}");
    }

    // 添加初始化方法
    public void Initialize(Transform rotorTransform)
    {
        rotor = rotorTransform;
        angle = 0f;
        angularVelocity = 0f;
        torqueInput = 0f;
    }
    
    public void Update(float deltaTime)
    {
        float frictionTorque = 0f;
        
        // 静摩擦处理
        if (Mathf.Abs(angularVelocity) < 0.1f && Mathf.Abs(torqueInput) < staticFriction)
        {
            frictionTorque = -torqueInput;
        }
        else
        {
            frictionTorque = -Mathf.Sign(angularVelocity) * staticFriction;
        }
        // Debug.Log(angularVelocity);
        // 总力矩计算
        float totalTorque = torqueInput 
                         - loadTorque * Mathf.Sign(angularVelocity)
                         - damping * angularVelocity 
                         + frictionTorque;
        
        // Debug.Log(totalTorque);
        
        // // 物理模拟
        float angularAccel = totalTorque / inertia;
        // Debug.Log(inertia);
        angularVelocity = 0;
        angularVelocity += angularAccel * deltaTime;
        angle += angularVelocity * deltaTime;
        
        // 更新转子模型
        if (rotor != null)
        {
            rotor.localRotation = Quaternion.Euler(0f, Mathf.Rad2Deg * angle, 0f);
        }
    }
}

public class MotorSim : MonoBehaviour
{
    [Header("Motor Units")]
    public Motor[] motors; // 电机单元数组

    [Header("Debug Control")]
    public bool useKeyboardControl = false;
    [Range(0, 10)] public int keyboardControlledMotorIndex = 0; // 键盘控制的电机索引
    public float keyboardControlTorque = 0.1f; // 键盘控制力矩大小
    
    void Start()
    {
        InitializeMotors();
    }

    private void InitializeMotors()
    {
        // 确保电机数组已创建
        if (motors == null || motors.Length == 0)
        {
            motors = new Motor[1];
            motors[0] = new Motor(0);
        }

        // 为每个电机找到对应的转子并初始化
        for (int i = 0; i < motors.Length; i++)
        {
            if (motors[i] == null)
            {
                motors[i] = new Motor((byte)i);
            }

            // // 查找对应的转子
            // Transform rotorTransform = transform.Find($"RotorPivot_{i}");
            // if (rotorTransform != null)
            // {
            //     motors[i].Initialize(rotorTransform);
            // }
            // else
            // {
            //     Debug.LogError($"找不到电机 {i} 的转子对象！请确保场景中存在 RotorPivot_{i}");
            // }
        }
    }
    
    
    
    void Update()
    {
        float deltaTime = Time.deltaTime;
        // Debug.Log(deltaTime);
        
        // 处理键盘输入
        // HandleKeyboardInput();
        
        // 更新所有电机单元
        foreach (var unit in motors)
        {
            unit.Update(deltaTime);
        }
    }

    void HandleKeyboardInput()
    {
        if (!useKeyboardControl || motors.Length == 0) return;
        
        int index = Mathf.Clamp(keyboardControlledMotorIndex, 0, motors.Length - 1);
        
        if (Keyboard.current.leftArrowKey.isPressed)
        {
            motors[index].torqueInput = -keyboardControlTorque;
        }
        else if (Keyboard.current.rightArrowKey.isPressed)
        {
            motors[index].torqueInput = keyboardControlTorque;
        }
        else
        {
            motors[index].torqueInput = 0f;
        }
    }

    #region Public API for MotorComServer
    public void SetTorque(byte motorId, float torque)
    {
        foreach (var unit in motors)
        {
            if (unit.motorId == motorId)
            {
                unit.torqueInput = torque;
                return;
            }
        }
        Debug.LogWarning($"Motor with ID {motorId} not found!");
    }

    public float GetAngle(byte motorId)
    {
        foreach (var unit in motors)
        {
            if (unit.motorId == motorId)
            {
                return unit.angle;
            }
        }
        Debug.LogWarning($"Motor with ID {motorId} not found!");
        return 0f;
    }

    public float GetSpeed(byte motorId)
    {
        foreach (var unit in motors)
        {
            if (unit.motorId == motorId)
            {
                return unit.angularVelocity;
            }
        }
        Debug.LogWarning($"Motor with ID {motorId} not found!");
        return 0f;
    }
    #endregion

    #region Editor Helpers
    [ContextMenu("Add New Motor Unit")]
    void AddNewMotorUnit()
    {
        int newIndex = (motors?.Length ?? 0);
        Array.Resize(ref motors, newIndex + 1);
        
        var newMotor = new Motor((byte)newIndex);
        motors[newIndex] = newMotor;

        // 尝试查找并设置转子
        Transform rotorTransform = transform.Find($"RotorPivot_{newIndex}");
        if (rotorTransform != null)
        {
            newMotor.Initialize(rotorTransform);
        }
    }
    
    void AddMotorUnits(int num)
    {
        int numMotorsToAdd = num; 

        int startIndex = (motors != null) ? motors.Length : 0;
        Array.Resize(ref motors, startIndex + numMotorsToAdd);

        for (int i = 0; i < numMotorsToAdd; i++)
        {
            motors[startIndex + i] = new Motor
            {
                motorId = (byte)(startIndex + i)
            };
        }

        Debug.Log($"Added {numMotorsToAdd} new motor units");
    }

    [ContextMenu("Print Motor Status")]
    void PrintMotorStatus()
    {
        string status = "=== Motor Status ===\n";
        foreach (var unit in motors)
        {
            status += $"Motor ID {unit.motorId}:\n";
            status += $"  Angle: {unit.angle * Mathf.Rad2Deg:F2}°\n";
            status += $"  Speed: {unit.angularVelocity:F2} rad/s\n";
            status += $"  Torque Input: {unit.torqueInput:F3} Nm\n";
        }
        Debug.Log(status);
    }
    #endregion
}