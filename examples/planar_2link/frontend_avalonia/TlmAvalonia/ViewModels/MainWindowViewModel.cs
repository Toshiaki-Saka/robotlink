// ViewModels/MainWindowViewModel.cs

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using Avalonia;
using Avalonia.Collections;
using Avalonia.Media;

using TlmAvalonia.Models;
using TlmAvalonia.Native;

namespace TlmAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    // ---- Plot geometry (kept in lock-step with MainWindow.axaml) ----
    private const double PlotWidthPx  = 580.0;
    private const double PlotHeightPx = 580.0;
    private const double PlotMargin   = 38.0;

    public MainWindowViewModel()
    {
        GridLines  = new AvaloniaList<GridLineMarker>();

        PlotBoxLeft   = PlotMargin;
        PlotBoxTop    = PlotMargin;
        PlotBoxWidth  = PlotWidthPx  - 2 * PlotMargin;
        PlotBoxHeight = PlotHeightPx - 2 * PlotMargin;

        ResetParametersToDefaults();
        // Initial angles: θ=0.
        Theta1 = 0.0;
        Theta2 = 0.0;
        RebuildAll();
    }

    // ===== Configuration =====
    private double _l1; public double L1 { get => _l1; set => SetField(ref _l1, value, runOnChange: true); }
    private double _l2; public double L2 { get => _l2; set => SetField(ref _l2, value, runOnChange: true); }
    private double _t1min; public double Theta1Min { get => _t1min; set => SetField(ref _t1min, value, runOnChange: true); }
    private double _t1max; public double Theta1Max { get => _t1max; set => SetField(ref _t1max, value, runOnChange: true); }
    private double _t2min; public double Theta2Min { get => _t2min; set => SetField(ref _t2min, value, runOnChange: true); }
    private double _t2max; public double Theta2Max { get => _t2max; set => SetField(ref _t2max, value, runOnChange: true); }

    // ===== Joint angles (driven by sliders) =====
    private double _theta1; public double Theta1 { get => _theta1; set => SetField(ref _theta1, value, runOnChange: true); }
    private double _theta2; public double Theta2 { get => _theta2; set => SetField(ref _theta2, value, runOnChange: true); }

    // ===== Plot render state =====
    private IReadOnlyList<BoundaryPolyline> _boundarySegments = System.Array.Empty<BoundaryPolyline>();
    public IReadOnlyList<BoundaryPolyline> BoundarySegments
    {
        get => _boundarySegments;
        private set => SetField(ref _boundarySegments, value);
    }
    public AvaloniaList<GridLineMarker>   GridLines  { get; }
    public double PlotBoxLeft   { get; }
    public double PlotBoxTop    { get; }
    public double PlotBoxWidth  { get; }
    public double PlotBoxHeight { get; }

    // ===== Arm geometry (mapped to canvas pixels) =====
    private Point _armPt0; public Point ArmPt0 { get => _armPt0; set => SetField(ref _armPt0, value); }
    private Point _armPt1; public Point ArmPt1 { get => _armPt1; set => SetField(ref _armPt1, value); }
    private Point _armPt2; public Point ArmPt2 { get => _armPt2; set => SetField(ref _armPt2, value); }

    // Marker top-left positions (Ellipse Canvas.Left/Top, 8px diameter).
    private double _nodeBaseX; public double NodeBaseX { get => _nodeBaseX; set => SetField(ref _nodeBaseX, value); }
    private double _nodeBaseY; public double NodeBaseY { get => _nodeBaseY; set => SetField(ref _nodeBaseY, value); }
    private double _nodeMidX;  public double NodeMidX  { get => _nodeMidX;  set => SetField(ref _nodeMidX,  value); }
    private double _nodeMidY;  public double NodeMidY  { get => _nodeMidY;  set => SetField(ref _nodeMidY,  value); }
    private double _nodeEndX;  public double NodeEndX  { get => _nodeEndX;  set => SetField(ref _nodeEndX,  value); }
    private double _nodeEndY;  public double NodeEndY  { get => _nodeEndY;  set => SetField(ref _nodeEndY,  value); }

    // ===== Tick labels (CLAUDE.md item 3) =====
    private string _refY0 = ""; public string RefY0 { get => _refY0; set => SetField(ref _refY0, value); }
    private string _refY25 = ""; public string RefY25 { get => _refY25; set => SetField(ref _refY25, value); }
    private string _refY50 = ""; public string RefY50 { get => _refY50; set => SetField(ref _refY50, value); }
    private string _refY75 = ""; public string RefY75 { get => _refY75; set => SetField(ref _refY75, value); }
    private string _refY100 = ""; public string RefY100 { get => _refY100; set => SetField(ref _refY100, value); }
    private string _refX0 = ""; public string RefX0 { get => _refX0; set => SetField(ref _refX0, value); }
    private string _refX25 = ""; public string RefX25 { get => _refX25; set => SetField(ref _refX25, value); }
    private string _refX50 = ""; public string RefX50 { get => _refX50; set => SetField(ref _refX50, value); }
    private string _refX75 = ""; public string RefX75 { get => _refX75; set => SetField(ref _refX75, value); }
    private string _refX100 = ""; public string RefX100 { get => _refX100; set => SetField(ref _refX100, value); }

    // ===== Readouts =====
    private string _theta1Text = "+0.000"; public string Theta1Text { get => _theta1Text; set => SetField(ref _theta1Text, value); }
    private string _theta2Text = "+0.000"; public string Theta2Text { get => _theta2Text; set => SetField(ref _theta2Text, value); }
    private string _xEndText = "+0.000"; public string XEndText { get => _xEndText; set => SetField(ref _xEndText, value); }
    private string _yEndText = "+0.000"; public string YEndText { get => _yEndText; set => SetField(ref _yEndText, value); }
    private string _statusMessage = "Ready"; public string StatusMessage { get => _statusMessage; set => SetField(ref _statusMessage, value); }

    // ===== Commands =====
    public void ResetAll()
    {
        ResetParametersToDefaults();
        Theta1 = 0.0;
        Theta2 = 0.0;
        // Setters above already trigger RebuildAll via SetField(runOnChange).
    }

    // ===== Internals =====
    private void ResetParametersToDefaults()
    {
        var d = TlmRobotConfig.Default();
        // SetField suppress runOnChange while we batch-set defaults by
        // writing directly to backing fields, then triggering one rebuild.
        _l1 = d.L1; _l2 = d.L2;
        _t1min = d.Theta1Min; _t1max = d.Theta1Max;
        _t2min = d.Theta2Min; _t2max = d.Theta2Max;
        OnPropertyChanged(nameof(L1));        OnPropertyChanged(nameof(L2));
        OnPropertyChanged(nameof(Theta1Min)); OnPropertyChanged(nameof(Theta1Max));
        OnPropertyChanged(nameof(Theta2Min)); OnPropertyChanged(nameof(Theta2Max));
    }

    private TlmRobotConfig CurrentConfig() => new()
    {
        L1 = L1, L2 = L2,
        Theta1Min = Theta1Min, Theta1Max = Theta1Max,
        Theta2Min = Theta2Min, Theta2Max = Theta2Max,
    };

    private void RebuildAll()
    {
        var cfg = CurrentConfig();
        // Clamp joint angles to the current limits (without re-triggering
        // RebuildAll — write directly to backing fields).
        double t1 = Math.Max(cfg.Theta1Min, Math.Min(cfg.Theta1Max, _theta1));
        double t2 = Math.Max(cfg.Theta2Min, Math.Min(cfg.Theta2Max, _theta2));
        if (t1 != _theta1) { _theta1 = t1; OnPropertyChanged(nameof(Theta1)); }
        if (t2 != _theta2) { _theta2 = t2; OnPropertyChanged(nameof(Theta2)); }

        TlmPose pose;
        WorkspaceSide wsMinus, wsPlus;
        try
        {
            pose    = TlmSolver.ForwardKinematics(cfg, t1, t2);
            wsMinus = TlmSolver.ComputeWorkspace(cfg, cfg.Theta2Min, 0.0, 100);
            wsPlus  = TlmSolver.ComputeWorkspace(cfg, 0.0, cfg.Theta2Max, 100);
        }
        catch (Exception ex)
        {
            StatusMessage = $"Bad config: {ex.Message}";
            return;
        }

        // ---- Plot range: square, ±extent ----
        double extent = (cfg.L1 + cfg.L2) * 1.1;
        if (extent <= 0) extent = 1.0;
        double xMin = -extent, xMax = extent;
        double yMin = -extent, yMax = extent;
        double dx = xMax - xMin, dy = yMax - yMin;

        Point Map(double x, double y) => new(
            PlotBoxLeft + (x - xMin) / dx * PlotBoxWidth,
            PlotBoxTop  + (1.0 - (y - yMin) / dy) * PlotBoxHeight);

        // ---- Boundary curves → direct-draw segments (WorkspaceOverlay) ----
        var redBrush   = (IBrush)new SolidColorBrush(Color.Parse("#DC322F"));
        var greenBrush = (IBrush)new SolidColorBrush(Color.Parse("#3CA03C"));
        var segs = new List<BoundaryPolyline>();
        void AddCurve(double[] xs, double[] ys, IBrush brush)
        {
            for (int i = 1; i < xs.Length; ++i)
                segs.Add(new BoundaryPolyline
                {
                    Start     = Map(xs[i - 1], ys[i - 1]),
                    End       = Map(xs[i],     ys[i]),
                    LineBrush = brush,
                });
        }
        AddCurve(wsMinus.AX, wsMinus.AY, redBrush);
        AddCurve(wsMinus.BX, wsMinus.BY, redBrush);
        AddCurve(wsMinus.CX, wsMinus.CY, redBrush);
        AddCurve(wsMinus.DX, wsMinus.DY, redBrush);
        AddCurve(wsPlus.AX,  wsPlus.AY,  greenBrush);
        AddCurve(wsPlus.BX,  wsPlus.BY,  greenBrush);
        AddCurve(wsPlus.CX,  wsPlus.CY,  greenBrush);
        AddCurve(wsPlus.DX,  wsPlus.DY,  greenBrush);
        BoundarySegments = segs;

        // ---- Arm pose ----
        ArmPt0 = Map(pose.BaseX,   pose.BaseY);
        ArmPt1 = Map(pose.Joint2X, pose.Joint2Y);
        ArmPt2 = Map(pose.EndX,    pose.EndY);
        // Marker top-left for an 8px ellipse centred on each node.
        NodeBaseX = ArmPt0.X - 4; NodeBaseY = ArmPt0.Y - 4;
        NodeMidX  = ArmPt1.X - 4; NodeMidY  = ArmPt1.Y - 4;
        // End-effector dot is a touch larger (12px), so offset by -6.
        NodeEndX  = ArmPt2.X - 6; NodeEndY  = ArmPt2.Y - 6;

        // ---- Grid lines (5x5) ----
        GridLines.Clear();
        for (int i = 0; i <= 4; ++i)
        {
            double f = i / 4.0;
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(PlotBoxLeft,                PlotBoxTop + f * PlotBoxHeight),
                End   = new Point(PlotBoxLeft + PlotBoxWidth, PlotBoxTop + f * PlotBoxHeight),
            });
            GridLines.Add(new GridLineMarker
            {
                Start = new Point(PlotBoxLeft + f * PlotBoxWidth, PlotBoxTop),
                End   = new Point(PlotBoxLeft + f * PlotBoxWidth, PlotBoxTop + PlotBoxHeight),
            });
        }

        // ---- Tick labels ----
        string Y(double frac) => Snap((1.0 - frac) * yMax + frac * yMin, dy);
        string X(double frac) => Snap(xMin + frac * dx, dx);
        RefY0 = Y(1.0); RefY25 = Y(0.75); RefY50 = Y(0.5); RefY75 = Y(0.25); RefY100 = Y(0.0);
        RefX0 = X(0.0); RefX25 = X(0.25); RefX50 = X(0.5); RefX75 = X(0.75); RefX100 = X(1.0);

        // ---- Readouts ----
        Theta1Text = t1.ToString("+0.000;-0.000;+0.000", CultureInfo.InvariantCulture);
        Theta2Text = t2.ToString("+0.000;-0.000;+0.000", CultureInfo.InvariantCulture);
        XEndText   = pose.EndX.ToString("+0.000;-0.000;+0.000", CultureInfo.InvariantCulture);
        YEndText   = pose.EndY.ToString("+0.000;-0.000;+0.000", CultureInfo.InvariantCulture);
        StatusMessage = string.Create(CultureInfo.InvariantCulture,
            $"theta1 = {t1:+0.000;-0.000;+0.000} rad, theta2 = {t2:+0.000;-0.000;+0.000} rad, " +
            $"end = ({pose.EndX:+0.000;-0.000;+0.000}, {pose.EndY:+0.000;-0.000;+0.000})");
    }

    private static string Snap(double v, double span)
    {
        double eps = span * 1e-9;
        if (Math.Abs(v) < eps) v = 0.0;
        return v.ToString("G4", CultureInfo.InvariantCulture);
    }

    // ===== INotifyPropertyChanged + auto-rebuild =====
    public event PropertyChangedEventHandler? PropertyChanged;
    private void OnPropertyChanged([CallerMemberName] string? name = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));

    private bool SetField<T>(ref T field, T value,
                              [CallerMemberName] string? name = null,
                              bool runOnChange = false)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        OnPropertyChanged(name);
        if (runOnChange) RebuildAll();
        return true;
    }
}
