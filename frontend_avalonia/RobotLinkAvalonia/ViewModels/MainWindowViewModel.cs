// ViewModels/MainWindowViewModel.cs

using System.ComponentModel;
using System.Runtime.CompilerServices;
using Avalonia.Media;
using RobotLinkAvalonia.Models;
using RobotLinkAvalonia.Views;

namespace RobotLinkAvalonia.ViewModels;

public sealed class MainWindowViewModel : INotifyPropertyChanged
{
    // matplotlib tab10 colours (C0..C3)
    private static readonly Color C0 = Color.Parse("#1F77B4");  // blue
    private static readonly Color C1 = Color.Parse("#FF7F0E");  // orange
    private static readonly Color C2 = Color.Parse("#2CA02C");  // green
    private static readonly Color C3 = Color.Parse("#DC143C");  // crimson (matches Python "crimson")

    // Joint-angle colours: same pair for all three joints, matching matplotlib defaults.
    // desired → blue C0 at ~70 % opacity (alpha=179); actual → orange C1 solid.
    private static readonly Color ActualColor  = C1;
    private static readonly Color DesiredColor = Color.FromArgb(179, 0x1F, 0x77, 0xB4);

    // ── Chart series properties (bound to ChartControl.Series) ───────────
    private IReadOnlyList<ChartSeries> _q1Series  = Array.Empty<ChartSeries>();
    private IReadOnlyList<ChartSeries> _q2Series  = Array.Empty<ChartSeries>();
    private IReadOnlyList<ChartSeries> _q3Series  = Array.Empty<ChartSeries>();
    private IReadOnlyList<ChartSeries> _torSeries = Array.Empty<ChartSeries>();
    private IReadOnlyList<ChartSeries> _errAng    = Array.Empty<ChartSeries>();
    private IReadOnlyList<ChartSeries> _errHand   = Array.Empty<ChartSeries>();

    public IReadOnlyList<ChartSeries> Q1Series  { get => _q1Series;  set => SetField(ref _q1Series,  value); }
    public IReadOnlyList<ChartSeries> Q2Series  { get => _q2Series;  set => SetField(ref _q2Series,  value); }
    public IReadOnlyList<ChartSeries> Q3Series  { get => _q3Series;  set => SetField(ref _q3Series,  value); }
    public IReadOnlyList<ChartSeries> TorSeries { get => _torSeries; set => SetField(ref _torSeries, value); }
    public IReadOnlyList<ChartSeries> ErrAng    { get => _errAng;    set => SetField(ref _errAng,    value); }
    public IReadOnlyList<ChartSeries> ErrHand   { get => _errHand;   set => SetField(ref _errHand,   value); }

    private string _statusText   = "No data loaded.";
    public  string StatusText   { get => _statusText;   set => SetField(ref _statusText,   value); }

    private string _errHandTitle = "Hand Position Error";
    public  string ErrHandTitle  { get => _errHandTitle; set => SetField(ref _errHandTitle, value); }

    private ArmFrame[] _armFrames = Array.Empty<ArmFrame>();
    public  ArmFrame[] ArmFrames  { get => _armFrames; private set => SetField(ref _armFrames, value); }

    // ── Public actions ────────────────────────────────────────────────────
    private SimData? _lastData;

    public void LoadCsv(string path)
    {
        try
        {
            var d = SimData.Load(path);
            _lastData = d;
            UpdateCharts(d);
            StatusText = $"Loaded: {System.IO.Path.GetFileName(path)}  ({d.Count} rows)";
        }
        catch (Exception ex)
        {
            StatusText = $"Load failed: {ex.Message}";
        }
    }

    public void RefreshCharts()
    {
        if (_lastData is not null)
            UpdateCharts(_lastData);
    }

    // ── Chart data builder ────────────────────────────────────────────────
    private void UpdateCharts(SimData d)
    {
        // Joint Angles — all joints share the same actual/desired colour pair (like matplotlib)
        var names   = new[] { "q1 (shoulder yaw)", "q2 (shoulder pitch)", "q3 (elbow)" };
        var actuals = new[] { d.Q1,  d.Q2,  d.Q3  };
        var desired = new[] { d.Qd1, d.Qd2, d.Qd3 };
        var series  = new IReadOnlyList<ChartSeries>[3];

        for (int i = 0; i < 3; i++)
        {
            series[i] = new ChartSeries[]
            {
                new() { Label = $"{names[i]} desired", Color = DesiredColor, Dashed = true,
                        Points = Zip(d.T, desired[i]) },
                new() { Label = $"{names[i]} actual",  Color = ActualColor,  Dashed = false,
                        Points = Zip(d.T, actuals[i]) },
            };
        }
        Q1Series = series[0]; Q2Series = series[1]; Q3Series = series[2];

        // Torques — tab10 C0/C1/C2 per joint (same as matplotlib default cycle)
        TorSeries = new ChartSeries[]
        {
            new() { Label = "τ1", Color = C0, Points = Zip(d.T, d.Tau1) },
            new() { Label = "τ2", Color = C1, Points = Zip(d.T, d.Tau2) },
            new() { Label = "τ3", Color = C2, Points = Zip(d.T, d.Tau3) },
        };

        // Joint errors (rad → deg) — labels "e1"/"e2"/"e3" matching PyQt6
        ErrAng = new ChartSeries[]
        {
            new() { Label = "e1", Color = C0, Points = Zip(d.T, SimData.ToDeg(d.Err1)) },
            new() { Label = "e2", Color = C1, Points = Zip(d.T, SimData.ToDeg(d.Err2)) },
            new() { Label = "e3", Color = C2, Points = Zip(d.T, SimData.ToDeg(d.Err3)) },
        };

        // Hand position error (mm) — crimson, no legend label, with t=2 s RMS title
        var handErr = d.HandError();
        ErrHand = new ChartSeries[]
        {
            new() { Label = "", Color = C3, Points = Zip(d.T, handErr) },
        };

        // Steady-state hand RMS error for t >= 2 s
        double sumSq = 0.0; int ssCnt = 0;
        for (int i = 0; i < d.Count; i++)
        {
            if (d.T[i] >= 2.0) { sumSq += handErr[i] * handErr[i]; ssCnt++; }
        }
        double rms = ssCnt > 0 ? Math.Sqrt(sumSq / ssCnt) : 0.0;
        ErrHandTitle = $"Steady-state hand RMS error: {rms:F3} mm";

        // Arm animation frames: compute elbow from joint angles (L2 = 0.5 m)
        const double L2 = 0.5;
        var frames = new ArmFrame[d.Count];
        for (int i = 0; i < d.Count; i++)
        {
            double c1 = Math.Cos(d.Q1[i]), s1 = Math.Sin(d.Q1[i]);
            double c2 = Math.Cos(d.Q2[i]), s2 = Math.Sin(d.Q2[i]);
            frames[i] = new ArmFrame(
                L2 * c1 * c2, L2 * s1 * c2, -L2 * s2,
                d.HandX[i],   d.HandY[i],   d.HandZ[i],
                d.HandDesX[i], d.HandDesY[i], d.HandDesZ[i],
                d.T[i]);
        }
        ArmFrames = frames;
    }

    private static (double, double)[] Zip(double[] xs, double[] ys)
    {
        int n = Math.Min(xs.Length, ys.Length);
        var r = new (double, double)[n];
        for (int i = 0; i < n; i++) r[i] = (xs[i], ys[i]);
        return r;
    }

    // ── INotifyPropertyChanged ────────────────────────────────────────────
    public event PropertyChangedEventHandler? PropertyChanged;
    private bool SetField<T>(ref T field, T value, [CallerMemberName] string? name = null)
    {
        if (EqualityComparer<T>.Default.Equals(field, value)) return false;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
        return true;
    }
}
