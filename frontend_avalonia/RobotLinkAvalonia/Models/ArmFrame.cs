namespace RobotLinkAvalonia.Models;

public readonly record struct ArmFrame(
    double Ex, double Ey, double Ez,
    double Hx, double Hy, double Hz,
    double Hdx, double Hdy, double Hdz,
    double T);
