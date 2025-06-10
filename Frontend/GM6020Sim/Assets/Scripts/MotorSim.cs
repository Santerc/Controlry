using UnityEngine;
using UnityEngine.InputSystem; // ✅ 新输入系统命名空间

public class MotorSim : MonoBehaviour
{
    [Header("Motor Components")]
    public Transform rotor;   // 拖入转子模型

    [Header("Control Input")]
    [SerializeField]
    private bool useKeyboardControl = false;  // 添加开关以控制输入来源
    [SerializeField]
    private float torqueInput = 0f;
    public float TorqueInput
    {
        get => torqueInput;
        set
        {
            // if (!useKeyboardControl)
                torqueInput = value;
        }
    }

    [Header("Mechanical Parameters")]
    public float inertia = 0.01f;     // 总转动惯量（转子 + 负载）
    public float damping = 0.002f;    // 粘性摩擦系数
    public float staticFriction = 0.05f; // 静摩擦力矩
    public float loadTorque = 0.02f;  // 恒定负载力矩

    private float angularVelocity = 0f; // rad/s
    private float angle = 0f;           // rad

    public float Angle => angle;
    public float Speed => angularVelocity;

    void Update()
    {
        float frictionTorque = 0f;

        // if (useKeyboardControl)
        // {
        //     if (Keyboard.current.leftArrowKey.isPressed)
        //         torqueInput = -0.1f;
        //     else if (Keyboard.current.rightArrowKey.isPressed)
        //         torqueInput = 0.1f;
        //     else
        //         torqueInput = 0f;
        // }

        if (Mathf.Abs(angularVelocity) < 0.1f && Mathf.Abs(torqueInput) < staticFriction)
        {
            frictionTorque = -torqueInput;
        }
        else
        {
            frictionTorque = -Mathf.Sign(angularVelocity) * staticFriction;
        }

        float totalTorque =
            torqueInput
            - loadTorque * Mathf.Sign(angularVelocity)
            - damping * angularVelocity
            + frictionTorque;

        float angularAccel = totalTorque / inertia;
        angularVelocity += angularAccel * Time.deltaTime;
        angle += angularVelocity * Time.deltaTime;

        if (rotor != null)
            rotor.localRotation = Quaternion.Euler(0f, Mathf.Rad2Deg * angle, 0f);
    }

    public float GetToque()
    {
        return torqueInput;
    }
}
