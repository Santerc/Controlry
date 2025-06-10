using UnityEngine;
using TMPro;
public class MotorUI : MonoBehaviour
{
    public MotorSim motorSim;
    public TextMeshProUGUI angleText;
    public TextMeshProUGUI speedText;
    public TextMeshProUGUI torqueText;

    private Color baseColor = new Color(1f, 1f, 1f, 0.9f); // 白色略透明
    private Color highlightColor = new Color(1f, 0.55f, 0f, 1f); // 大疆橙色

    void Update()
    {
        if (motorSim == null) return;

        angleText.text = $"Angle=: {motorSim.Angle * Mathf.Rad2Deg:F2} °";
        speedText.text = $"Omega=: {motorSim.Speed:F2} rad/s";
        torqueText.text = $"Togue=: {motorSim.GetToque():F3} Nm";

        // 让转矩数字周期性渐变颜色
        float t = (Mathf.Sin(Time.time * 3f) + 1f) / 2f;
        torqueText.color = Color.Lerp(baseColor, highlightColor, t);
    }

}
