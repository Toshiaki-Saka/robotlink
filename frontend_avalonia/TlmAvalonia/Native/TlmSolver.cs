// Native/TlmSolver.cs — managed wrapper around tlm_core.

using System;
using System.Runtime.InteropServices;

namespace TlmAvalonia.Native;

public sealed class TlmRobotConfig
{
    public double L1         { get; set; } = 1.0;
    public double L2         { get; set; } = 1.0;
    public double Theta1Min  { get; set; } = -Math.PI / 2;
    public double Theta1Max  { get; set; } =  Math.PI / 2;
    public double Theta2Min  { get; set; } = -Math.PI / 2;
    public double Theta2Max  { get; set; } =  Math.PI / 2;
    public double BaseX      { get; set; } = 0.0;
    public double BaseY      { get; set; } = 0.0;

    public static TlmRobotConfig Default()
    {
        var n = new TlmConfigNative();
        TlmCoreNative.DefaultConfig(ref n);
        return new TlmRobotConfig
        {
            L1 = n.L1, L2 = n.L2,
            Theta1Min = n.Theta1Min, Theta1Max = n.Theta1Max,
            Theta2Min = n.Theta2Min, Theta2Max = n.Theta2Max,
            BaseX = n.BaseX, BaseY = n.BaseY,
        };
    }

    internal TlmConfigNative ToNative() => new()
    {
        L1 = L1, L2 = L2,
        Theta1Min = Theta1Min, Theta1Max = Theta1Max,
        Theta2Min = Theta2Min, Theta2Max = Theta2Max,
        BaseX = BaseX, BaseY = BaseY,
    };
}

public sealed class TlmPose
{
    public required double BaseX   { get; init; }
    public required double BaseY   { get; init; }
    public required double Joint2X { get; init; }
    public required double Joint2Y { get; init; }
    public required double EndX    { get; init; }
    public required double EndY    { get; init; }
}

public sealed class WorkspaceSide
{
    public required double[] AX { get; init; }
    public required double[] AY { get; init; }
    public required double[] BX { get; init; }
    public required double[] BY { get; init; }
    public required double[] CX { get; init; }
    public required double[] CY { get; init; }
    public required double[] DX { get; init; }
    public required double[] DY { get; init; }
}

public static class TlmSolver
{
    public static string Version()
    {
        var p = TlmCoreNative.Version();
        return Marshal.PtrToStringAnsi(p) ?? "tlm_core (unknown)";
    }

    public static TlmPose ForwardKinematics(TlmRobotConfig cfg, double t1, double t2)
    {
        var n = cfg.ToNative();
        var p = new TlmPoseNative();
        if (TlmCoreNative.ForwardKinematics(ref n, t1, t2, ref p) == 0)
            throw new InvalidOperationException("forward_kinematics rejected the inputs");
        return new TlmPose
        {
            BaseX = p.BaseX, BaseY = p.BaseY,
            Joint2X = p.Joint2X, Joint2Y = p.Joint2Y,
            EndX = p.EndX, EndY = p.EndY,
        };
    }

    public static WorkspaceSide ComputeWorkspace(TlmRobotConfig cfg,
                                                  double t2Lo, double t2Hi,
                                                  int samplesPerCurve = 100)
    {
        var n = cfg.ToNative();
        var handle = TlmCoreNative.ComputeWorkspace(ref n, t2Lo, t2Hi, samplesPerCurve);
        if (handle == IntPtr.Zero)
            throw new InvalidOperationException("compute_workspace rejected the inputs");
        try
        {
            int nn = TlmCoreNative.WsSamplesPerCurve(handle);
            double[] mk() => new double[nn];
            var ax = mk(); var ay = mk(); var bx = mk(); var by = mk();
            var cx = mk(); var cy = mk(); var dx = mk(); var dy = mk();
            TlmCoreNative.WsCopyAX(handle, ax, nn);
            TlmCoreNative.WsCopyAY(handle, ay, nn);
            TlmCoreNative.WsCopyBX(handle, bx, nn);
            TlmCoreNative.WsCopyBY(handle, by, nn);
            TlmCoreNative.WsCopyCX(handle, cx, nn);
            TlmCoreNative.WsCopyCY(handle, cy, nn);
            TlmCoreNative.WsCopyDX(handle, dx, nn);
            TlmCoreNative.WsCopyDY(handle, dy, nn);
            return new WorkspaceSide
            {
                AX = ax, AY = ay, BX = bx, BY = by,
                CX = cx, CY = cy, DX = dx, DY = dy,
            };
        }
        finally { TlmCoreNative.FreeWorkspace(handle); }
    }
}
