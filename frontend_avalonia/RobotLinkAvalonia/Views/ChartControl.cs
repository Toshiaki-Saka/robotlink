// Views/ChartControl.cs — custom Avalonia control that renders time-series charts.
//
// Design goals:
//   • Grid lines drawn BEFORE curves (they appear behind data, never on top)
//   • Legend rendered in upper-right corner with semi-transparent background
//   • Robust against empty data (shows placeholder text)
//   • No external plotting library dependency

using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace RobotLinkAvalonia.Views;

public sealed class ChartSeries
{
    public string Label   { get; init; } = "";
    public Color  Color   { get; init; } = Colors.SteelBlue;
    public bool   Dashed  { get; init; } = false;
    public (double x, double y)[] Points { get; init; } = Array.Empty<(double, double)>();
}

public sealed class ChartControl : Control
{
    // ── Styled properties (changes trigger redraw via AffectsRender) ──────
    public static readonly StyledProperty<string?> TitleProperty =
        AvaloniaProperty.Register<ChartControl, string?>(nameof(Title));

    public static readonly StyledProperty<string?> XLabelProperty =
        AvaloniaProperty.Register<ChartControl, string?>(nameof(XLabel));

    public static readonly StyledProperty<string?> YLabelProperty =
        AvaloniaProperty.Register<ChartControl, string?>(nameof(YLabel));

    public static readonly StyledProperty<IReadOnlyList<ChartSeries>?> SeriesProperty =
        AvaloniaProperty.Register<ChartControl, IReadOnlyList<ChartSeries>?>(nameof(Series));

    public static readonly StyledProperty<double> VLineXProperty =
        AvaloniaProperty.Register<ChartControl, double>(nameof(VLineX), double.NaN);

    static ChartControl()
    {
        AffectsRender<ChartControl>(TitleProperty, XLabelProperty, YLabelProperty, SeriesProperty, VLineXProperty);
    }

    public string?                      Title   { get => GetValue(TitleProperty);   set => SetValue(TitleProperty,   value); }
    public string?                      XLabel  { get => GetValue(XLabelProperty);  set => SetValue(XLabelProperty,  value); }
    public string?                      YLabel  { get => GetValue(YLabelProperty);  set => SetValue(YLabelProperty,  value); }
    public IReadOnlyList<ChartSeries>?  Series  { get => GetValue(SeriesProperty);  set => SetValue(SeriesProperty,  value); }
    public double                       VLineX  { get => GetValue(VLineXProperty);  set => SetValue(VLineXProperty,  value); }

    // ── Colors / pens ────────────────────────────────────────────────────
    private static readonly IBrush BackBrush   = Brushes.White;
    private static readonly IBrush GridBrush   = new SolidColorBrush(Color.FromRgb(210, 210, 210));
    private static readonly IBrush TickBrush   = Brushes.Black;
    private static readonly IBrush LegBgBrush  = new SolidColorBrush(Color.FromArgb(210, 255, 255, 255));
    private static readonly IBrush LegBdrBrush = new SolidColorBrush(Color.FromRgb(160, 160, 160));

    private static readonly Pen GridPen    = new(GridBrush,   1.0);
    private static readonly Pen BoxPen     = new(Brushes.Black, 1.0);
    private static readonly Pen LegBdrPen  = new(LegBdrBrush, 0.7);

    private static readonly Typeface  TickTypeface  = new(FontFamily.Default);
    private static readonly Typeface  LabelTypeface = new(FontFamily.Default);
    private static readonly Typeface  TitleTypeface = new(FontFamily.Default, FontStyle.Normal, FontWeight.Bold);

    // ── Render ────────────────────────────────────────────────────────────
    public override void Render(DrawingContext ctx)
    {
        var bounds = Bounds;

        // 1. Background
        ctx.FillRectangle(BackBrush, new Rect(bounds.Size));

        var series = Series;
        if (series is null || series.Count == 0 || series.All(s => s.Points.Length == 0))
        {
            DrawPlaceholder(ctx, bounds, "(データなし)");
            return;
        }

        // 2. Margins
        bool hasTitle  = !string.IsNullOrEmpty(Title);
        bool hasXLabel = !string.IsNullOrEmpty(XLabel);
        bool hasYLabel = !string.IsNullOrEmpty(YLabel);

        double mL = hasYLabel ? 68 : 58, mR = 14, mT = hasTitle ? 26 : 8, mB = hasXLabel ? 36 : 24;
        double plotW = Math.Max(1, bounds.Width  - mL - mR);
        double plotH = Math.Max(1, bounds.Height - mT - mB);
        var plotRect = new Rect(mL, mT, plotW, plotH);

        // 3. Axis ranges
        double xMin = double.MaxValue, xMax = double.MinValue;
        double yMin = double.MaxValue, yMax = double.MinValue;
        foreach (var s in series)
        {
            foreach (var (x, y) in s.Points)
            {
                if (x < xMin) xMin = x; if (x > xMax) xMax = x;
                if (y < yMin) yMin = y; if (y > yMax) yMax = y;
            }
        }
        ExpandRange(ref xMin, ref xMax);
        ExpandRange(ref yMin, ref yMax);

        double xSpan = xMax - xMin;
        double ySpan = yMax - yMin;

        double XPx(double x) => mL + (x - xMin) / xSpan * plotW;
        double YPx(double y) => mT + (1.0 - (y - yMin) / ySpan) * plotH;

        // ── 4. Grid lines (drawn BEFORE curves) ──────────────────────────
        const int GridN = 5;
        for (int i = 0; i <= GridN; i++)
        {
            double fy = (double)i / GridN;
            double fx = (double)i / GridN;
            // Horizontal
            ctx.DrawLine(GridPen,
                new Point(plotRect.Left,  plotRect.Top + fy * plotH),
                new Point(plotRect.Right, plotRect.Top + fy * plotH));
            // Vertical
            ctx.DrawLine(GridPen,
                new Point(plotRect.Left + fx * plotW, plotRect.Top),
                new Point(plotRect.Left + fx * plotW, plotRect.Bottom));
        }

        // ── 5. Plot border ────────────────────────────────────────────────
        ctx.DrawRectangle(null, BoxPen, plotRect);

        // ── 6. Tick labels ────────────────────────────────────────────────
        const double tickFontSz = 9;
        for (int i = 0; i <= GridN; i++)
        {
            double fy = (double)i / GridN;
            double yv = yMax - fy * ySpan;
            var ft = MakeText($"{yv:G3}", tickFontSz, TickBrush);
            double tx = plotRect.Left - ft.Width - 4;
            double ty = plotRect.Top + fy * plotH - ft.Height / 2;
            ctx.DrawText(ft, new Point(tx, ty));

            double fx = (double)i / GridN;
            double xv = xMin + fx * xSpan;
            var ftx = MakeText($"{xv:G3}", tickFontSz, TickBrush);
            double px = plotRect.Left + fx * plotW - ftx.Width / 2;
            ctx.DrawText(ftx, new Point(px, plotRect.Bottom + 3));
        }

        // ── 6b. Axis labels ───────────────────────────────────────────────
        if (!string.IsNullOrEmpty(XLabel))
        {
            var ft = MakeText(XLabel!, 9.5, TickBrush);
            ctx.DrawText(ft, new Point(plotRect.Left + (plotW - ft.Width) / 2,
                                       plotRect.Bottom + 18));
        }

        if (!string.IsNullOrEmpty(YLabel))
        {
            var ft = MakeText(YLabel!, 9.5, TickBrush);
            // Rotate -90° (counterclockwise) and centre on the left of the plot.
            // Matrix convention: vectors are row vectors, so M1*M2 applies M1 first.
            double cx = 8;
            double cy = mT + plotH / 2;
            var m = Matrix.CreateRotation(-Math.PI / 2)
                  * Matrix.CreateTranslation(cx, cy);
            using (ctx.PushTransform(m))
                ctx.DrawText(ft, new Point(-ft.Width / 2, -ft.Height / 2));
        }

        // ── 6c. Title ─────────────────────────────────────────────────────
        if (!string.IsNullOrEmpty(Title))
        {
            var ft = new FormattedText(Title!,
                CultureInfo.InvariantCulture, FlowDirection.LeftToRight,
                TitleTypeface, 10, TickBrush);
            ctx.DrawText(ft, new Point((bounds.Width - ft.Width) / 2, 4));
        }

        // ── 4b. Vertical reference line ───────────────────────────────────
        double vlineX = VLineX;
        if (!double.IsNaN(vlineX) && vlineX >= xMin && vlineX <= xMax)
        {
            double vpx = XPx(vlineX);
            var vlinePen = new Pen(new SolidColorBrush(Color.FromRgb(128, 128, 128)), 1.0,
                                  new DashStyle(new double[] { 5, 3 }, 0));
            ctx.DrawLine(vlinePen, new Point(vpx, plotRect.Top), new Point(vpx, plotRect.Bottom));
        }

        // ── 7. Curves (clipped to plot area) ─────────────────────────────
        using (ctx.PushClip(plotRect.Inflate(1)))
        {
            foreach (var s in series)
            {
                if (s.Points.Length < 2) continue;
                DrawCurve(ctx, s, XPx, YPx);
            }
        }

        // ── 8. Legend ─────────────────────────────────────────────────────
        var labeled = series.Where(s => !string.IsNullOrEmpty(s.Label)).ToList();
        if (labeled.Count > 0)
            DrawLegend(ctx, labeled, plotRect);
    }

    // ── helpers ───────────────────────────────────────────────────────────
    private static void ExpandRange(ref double lo, ref double hi)
    {
        if (double.IsInfinity(lo) || double.IsNaN(lo)) { lo = -1; hi = 1; return; }
        double span = hi - lo;
        if (span < 1e-9) { lo -= 0.5; hi += 0.5; }
        else { double pad = 0.08 * span; lo -= pad; hi += pad; }
    }

    private static FormattedText MakeText(string s, double size, IBrush brush, Typeface? tf = null)
        => new(s, CultureInfo.InvariantCulture, FlowDirection.LeftToRight,
               tf ?? TickTypeface, size, brush);

    private static void DrawCurve(DrawingContext ctx, ChartSeries s,
                                  Func<double, double> xPx, Func<double, double> yPx)
    {
        var pen = new Pen(new SolidColorBrush(s.Color), 1.6,
                          s.Dashed ? new DashStyle(new double[] { 6, 3 }, 0) : null);

        var geo = new StreamGeometry();
        using (var sgc = geo.Open())
        {
            sgc.BeginFigure(new Point(xPx(s.Points[0].x), yPx(s.Points[0].y)), false);
            for (int i = 1; i < s.Points.Length; i++)
                sgc.LineTo(new Point(xPx(s.Points[i].x), yPx(s.Points[i].y)));
            sgc.EndFigure(false);
        }
        ctx.DrawGeometry(null, pen, geo);
    }

    private static void DrawLegend(DrawingContext ctx, List<ChartSeries> labeled, Rect plotRect)
    {
        const double swW   = 18;   // swatch width
        const double gap   =  4;
        const double lh    = 16;   // row height
        const double vpad  =  4;
        const double hpad  =  6;
        const double fs    =  9;

        double maxTw = labeled.Max(s => MakeText(s.Label, fs, Brushes.Black).Width);
        double boxW = swW + gap + maxTw + hpad * 2;
        double boxH = labeled.Count * lh + vpad * 2;

        double lx = plotRect.Right  - boxW - 8;
        double ly = plotRect.Top    + 6;

        // Background + border
        ctx.FillRectangle(LegBgBrush, new Rect(lx, ly, boxW, boxH));
        ctx.DrawRectangle(null, LegBdrPen, new Rect(lx, ly, boxW, boxH));

        double ry = ly + vpad;
        foreach (var s in labeled)
        {
            // Line swatch
            var swPen = new Pen(new SolidColorBrush(s.Color), 2.0,
                                s.Dashed ? new DashStyle(new double[] { 5, 2 }, 0) : null);
            ctx.DrawLine(swPen,
                new Point(lx + hpad,         ry + lh / 2),
                new Point(lx + hpad + swW,   ry + lh / 2));

            // Label text
            var ft = MakeText(s.Label, fs, Brushes.Black);
            ctx.DrawText(ft, new Point(lx + hpad + swW + gap, ry + (lh - ft.Height) / 2));
            ry += lh;
        }
    }

    private static void DrawPlaceholder(DrawingContext ctx, Rect bounds, string msg)
    {
        var ft = MakeText(msg, 11, Brushes.Gray);
        ctx.DrawText(ft, new Point((bounds.Width - ft.Width) / 2,
                                   (bounds.Height - ft.Height) / 2));
    }
}
