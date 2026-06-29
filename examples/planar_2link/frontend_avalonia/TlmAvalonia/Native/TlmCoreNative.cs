// Native/TlmCoreNative.cs — raw P/Invoke declarations for tlm_core.

using System;
using System.Runtime.InteropServices;

namespace TlmAvalonia.Native;

[StructLayout(LayoutKind.Sequential)]
public struct TlmConfigNative
{
    public double L1;
    public double L2;
    public double Theta1Min;
    public double Theta1Max;
    public double Theta2Min;
    public double Theta2Max;
    public double BaseX;
    public double BaseY;
}

[StructLayout(LayoutKind.Sequential)]
public struct TlmPoseNative
{
    public double BaseX, BaseY;
    public double Joint2X, Joint2Y;
    public double EndX, EndY;
}

internal static class TlmCoreNative
{
    public const string Lib = "tlm_core";

    [DllImport(Lib, EntryPoint = "tlm_core_version",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr Version();

    [DllImport(Lib, EntryPoint = "tlm_core_default_config",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void DefaultConfig(ref TlmConfigNative cfg);

    [DllImport(Lib, EntryPoint = "tlm_core_forward_kinematics",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int ForwardKinematics(ref TlmConfigNative cfg,
                                                double t1, double t2,
                                                ref TlmPoseNative outPose);

    [DllImport(Lib, EntryPoint = "tlm_core_compute_workspace",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr ComputeWorkspace(ref TlmConfigNative cfg,
                                                  double t2Lo, double t2Hi,
                                                  int samplesPerCurve);

    [DllImport(Lib, EntryPoint = "tlm_core_free_workspace",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern void FreeWorkspace(IntPtr handle);

    [DllImport(Lib, EntryPoint = "tlm_core_ws_samples_per_curve",
               CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsSamplesPerCurve(IntPtr handle);

    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_a_x", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyAX(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_a_y", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyAY(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_b_x", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyBX(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_b_y", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyBY(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_c_x", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyCX(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_c_y", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyCY(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_d_x", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyDX(IntPtr h, [Out] double[] b, int n);
    [DllImport(Lib, EntryPoint = "tlm_core_ws_copy_d_y", CallingConvention = CallingConvention.Cdecl)]
    public static extern int WsCopyDY(IntPtr h, [Out] double[] b, int n);
}
