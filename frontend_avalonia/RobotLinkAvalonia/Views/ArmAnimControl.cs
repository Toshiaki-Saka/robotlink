using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;
using RobotLinkAvalonia.Models;

namespace RobotLinkAvalonia.Views;

public sealed class ArmAnimControl : Control
{
    private ArmFrame[] _frames = Array.Empty<ArmFrame>();
    private int        _frame  = 0;
    private const int  Stride  = 6; // ~30 fps at dt=0.005
    private readonly DispatcherTimer _timer;

    public ArmAnimControl()
    {
        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(30) };
        _timer.Tick += (_, _) => Advance();
    }

    public void SetData(ArmFrame[] frames)
    {
        _timer.Stop();
        _frames = frames;
        _frame  = 0;
        InvalidateVisual();
    }

    public void Play()
    {
        if (_frames.Length > 0 && !_timer.IsEnabled)
            _timer.Start();
    }

    public void Stop() => _timer.Stop();

    private void Advance()
    {
        if (_frames.Length == 0) return;
        _frame = (_frame + Stride) % _frames.Length;
        InvalidateVisual();
    }

    private Point Project(double x, double y, double z)
    {
        // Matches matplotlib Axes3D default: azimuth=-60°, elevation=30°.
        // Rotate world by +60° around Z (camera azimuth -60° → world +60°):
        //   x' = 0.5x − 0.866y,  y' = 0.866x + 0.5y
        // screen-right: u = x'              = 0.500x − 0.866y
        // screen-down:  v = sin(30°)y' − cos(30°)z = 0.433x + 0.250y − 0.866z
        double u =  0.500 * x - 0.866 * y;
        double v =  0.433 * x + 0.250 * y - 0.866 * z;
        double scale = Math.Min(Bounds.Width, Bounds.Height) * 0.38;
        return new Point(Bounds.Width  / 2 + scale * u,
                         Bounds.Height / 2 + scale * (v - 0.10));
    }

    public override void Render(DrawingContext ctx)
    {
        ctx.FillRectangle(Brushes.White, new Rect(Bounds.Size));

        // Title
        var titleFt = MakeText("3-DOF Arm Trajectory Tracking", 10, Brushes.Black,
                               new Typeface(FontFamily.Default, FontStyle.Normal, FontWeight.Bold));
        ctx.DrawText(titleFt, new Point((Bounds.Width - titleFt.Width) / 2, 4));

        if (_frames.Length == 0)
        {
            var ph = MakeText("(no data)", 11, Brushes.Gray);
            ctx.DrawText(ph, new Point((Bounds.Width - ph.Width) / 2,
                                       (Bounds.Height - ph.Height) / 2));
            return;
        }

        var f = _frames[_frame];

        // Bounding box — mirrors Qt6: xlim/ylim=(-0.9,0.9), zlim=(-0.54,0.9)
        const double XN = -0.9, XX = 0.9;
        const double YN = -0.9, YX = 0.9;
        const double ZN = -0.54, ZX = 0.9;

        // matplotlib-style gray pane fills + white grid (3 faces: floor, back wall, right wall)
        var paneColor    = new SolidColorBrush(Color.FromRgb(236, 236, 236));
        var paneEdgePen  = new Pen(new SolidColorBrush(Color.FromRgb(196, 196, 196)), 0.8);
        var gridPen      = new Pen(Brushes.White, 0.8);

        DrawPane(ctx, paneColor, paneEdgePen,
            Project(XN,YN,ZN), Project(XX,YN,ZN), Project(XX,YX,ZN), Project(XN,YX,ZN)); // floor  (z=ZN)
        DrawPane(ctx, paneColor, paneEdgePen,
            Project(XN,YX,ZN), Project(XX,YX,ZN), Project(XX,YX,ZX), Project(XN,YX,ZX)); // back wall (y=YX)
        DrawPane(ctx, paneColor, paneEdgePen,
            Project(XX,YN,ZN), Project(XX,YX,ZN), Project(XX,YX,ZX), Project(XX,YN,ZX)); // right wall (x=XX)

        // Floor grid (XY at z=ZN) — -0.75..0.75 step 0.25 (matches matplotlib auto-ticks)
        for (double v = -0.75; v <= 0.76; v += 0.25)
        {
            ctx.DrawLine(gridPen, Project(v,  YN, ZN), Project(v,  YX, ZN));
            ctx.DrawLine(gridPen, Project(XN, v,  ZN), Project(XX, v,  ZN));
        }
        // Back wall (y=YX) grid — X: -0.75..0.75 step 0.25, Z: -0.4..0.8 step 0.2
        for (double v = -0.75; v <= 0.76; v += 0.25)
            ctx.DrawLine(gridPen, Project(v,  YX, ZN), Project(v,  YX, ZX));
        for (double v = -0.4; v <= 0.81; v += 0.2)
            ctx.DrawLine(gridPen, Project(XN, YX, v),  Project(XX, YX, v));
        // Right wall (x=XX) grid — Y: -0.75..0.75 step 0.25, Z: -0.4..0.8 step 0.2
        for (double v = -0.75; v <= 0.76; v += 0.25)
            ctx.DrawLine(gridPen, Project(XX, v,  ZN), Project(XX, v,  ZX));
        for (double v = -0.4; v <= 0.81; v += 0.2)
            ctx.DrawLine(gridPen, Project(XX, YN, v),  Project(XX, YX, v));

        // Axis box edges (dark, thin) — mirrors Qt6 section 3
        {
            var edgePen = new Pen(new SolidColorBrush(Color.FromRgb(50, 50, 50)), 0.8);
            // Bottom rectangle
            ctx.DrawLine(edgePen, Project(XN,YN,ZN), Project(XX,YN,ZN));
            ctx.DrawLine(edgePen, Project(XX,YN,ZN), Project(XX,YX,ZN));
            ctx.DrawLine(edgePen, Project(XX,YX,ZN), Project(XN,YX,ZN));
            ctx.DrawLine(edgePen, Project(XN,YX,ZN), Project(XN,YN,ZN));
            // Top edges (all four)
            ctx.DrawLine(edgePen, Project(XN,YN,ZX), Project(XX,YN,ZX));
            ctx.DrawLine(edgePen, Project(XN,YN,ZX), Project(XN,YX,ZX));
            ctx.DrawLine(edgePen, Project(XN,YX,ZX), Project(XX,YX,ZX));
            ctx.DrawLine(edgePen, Project(XX,YN,ZX), Project(XX,YX,ZX));
            // Vertical edges (all four visible corners)
            ctx.DrawLine(edgePen, Project(XN,YN,ZN), Project(XN,YN,ZX));
            ctx.DrawLine(edgePen, Project(XN,YX,ZN), Project(XN,YX,ZX));
            ctx.DrawLine(edgePen, Project(XX,YX,ZN), Project(XX,YX,ZX));
            ctx.DrawLine(edgePen, Project(XX,YN,ZN), Project(XX,YN,ZX));
        }

        // Axis tick labels — mirrors Qt6 section 3b
        {
            var tkBrush = new SolidColorBrush(Color.FromRgb(60, 60, 60));
            var axBf    = new Typeface(FontFamily.Default, FontStyle.Normal, FontWeight.Bold);
            const double tkSz = 6.5;

            // X ticks — back-bottom edge (y=YX, z=ZN); offset outward in +y
            foreach (double tv in new[]{-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75})
            {
                var tp  = Project(tv, YX + 0.15, ZN - 0.04);
                var lbl = MakeText(tv.ToString("F2", CultureInfo.InvariantCulture), tkSz, tkBrush);
                ctx.DrawText(lbl, new Point(tp.X - lbl.Width / 2, tp.Y - lbl.Height / 2));
            }
            // Y ticks — right-bottom edge (x=XX, z=ZN); offset outward in +x
            foreach (double tv in new[]{-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75})
            {
                var tp  = Project(XX + 0.13, tv, ZN - 0.04);
                var lbl = MakeText(tv.ToString("F2", CultureInfo.InvariantCulture), tkSz, tkBrush);
                ctx.DrawText(lbl, new Point(tp.X + 2, tp.Y - lbl.Height / 2));
            }
            // Z ticks — front-right vertical edge (x=XX, y=YN); offset outward in +x
            foreach (double tv in new[]{-0.4,-0.2,0.0,0.2,0.4,0.6,0.8})
            {
                var tp  = Project(XX + 0.13, YN - 0.04, tv);
                var lbl = MakeText(tv.ToString("F1", CultureInfo.InvariantCulture), tkSz, tkBrush);
                ctx.DrawText(lbl, new Point(tp.X + 2, tp.Y - lbl.Height / 2));
            }

            // Axis name labels — mirrors Qt6 section 4
            var tkBrush8 = new SolidColorBrush(Color.FromRgb(50, 50, 50));
            var xFt = MakeText("X", 8, tkBrush8);
            var xLp = Project(0, YX + 0.20, ZN);
            ctx.DrawText(xFt, new Point(xLp.X, xLp.Y - xFt.Height / 2));

            var yFt = MakeText("Y", 8, tkBrush8);
            var yLp = Project(XX + 0.20, 0, ZN);
            ctx.DrawText(yFt, new Point(yLp.X, yLp.Y - yFt.Height / 2));

            var zFt = MakeText("Z", 8, tkBrush8);
            var zLp = Project(XX, YN, ZX + 0.14);
            ctx.DrawText(zFt, new Point(zLp.X + 2, zLp.Y - zFt.Height / 2));
        }

        // Trajectories up to current frame
        if (_frame > 0)
        {
            var actualPen = new Pen(new SolidColorBrush(Color.Parse("#FF7F0E")), 1.2);
            var desPen    = new Pen(new SolidColorBrush(Color.Parse("#2CA02C")), 1.2,
                                    new DashStyle(new double[] { 5, 2 }, 0));
            for (int i = 1; i <= _frame; i++)
            {
                ctx.DrawLine(actualPen,
                    Project(_frames[i-1].Hx,  _frames[i-1].Hy,  _frames[i-1].Hz),
                    Project(_frames[i].Hx,    _frames[i].Hy,    _frames[i].Hz));
                ctx.DrawLine(desPen,
                    Project(_frames[i-1].Hdx, _frames[i-1].Hdy, _frames[i-1].Hdz),
                    Project(_frames[i].Hdx,   _frames[i].Hdy,   _frames[i].Hdz));
            }
        }

        // Arm links
        var p0    = Project(0,    0,    0   );
        var elbow = Project(f.Ex, f.Ey, f.Ez);
        var hand  = Project(f.Hx, f.Hy, f.Hz);

        var armPen = new Pen(new SolidColorBrush(Color.Parse("#1F77B4")), 4,
                             lineCap: PenLineCap.Round);
        ctx.DrawLine(armPen, p0, elbow);
        ctx.DrawLine(armPen, elbow, hand);

        var jointBrush = new SolidColorBrush(Color.Parse("#1F77B4"));
        foreach (var pt in new[] { p0, elbow, hand })
            ctx.DrawEllipse(jointBrush, null, pt, 5.5, 5.5);

        // Time label
        var timeFt = MakeText($"t = {f.T:F2} s", 8.5, Brushes.Black);
        ctx.DrawText(timeFt, new Point(8, 22));

        // Legend — upper right, matching Python/matplotlib placement
        var legFtA  = MakeText("actual hand path",  7.5, Brushes.Black);
        var legFtD  = MakeText("desired hand path", 7.5, Brushes.Black);
        const double swW = 22, swGap = 4, legPad = 6;
        double legTxtW = Math.Max(legFtA.Width, legFtD.Width);
        double legW  = legPad + swW + swGap + legTxtW + legPad;
        double legH  = legPad + 16 + 16 + legPad;
        double lx    = Bounds.Width  - legW - 8;
        double ly    = 8;

        // Background box
        ctx.FillRectangle(new SolidColorBrush(Color.FromArgb(210, 255, 255, 255)),
                          new Rect(lx, ly, legW, legH));
        ctx.DrawRectangle(null,
                          new Pen(new SolidColorBrush(Color.FromRgb(160, 160, 160)), 0.7),
                          new Rect(lx, ly, legW, legH));

        double rx = lx + legPad;
        double ry = ly + legPad + 8;   // first row centre

        var orangePen = new Pen(new SolidColorBrush(Color.Parse("#FF7F0E")), 1.5);
        ctx.DrawLine(orangePen, new Point(rx, ry), new Point(rx + swW, ry));
        ctx.DrawText(legFtA, new Point(rx + swW + swGap, ry - legFtA.Height / 2));

        ry += 16;
        var greenDash = new Pen(new SolidColorBrush(Color.Parse("#2CA02C")), 1.5,
                                new DashStyle(new double[] { 5, 2 }, 0));
        ctx.DrawLine(greenDash, new Point(rx, ry), new Point(rx + swW, ry));
        ctx.DrawText(legFtD, new Point(rx + swW + swGap, ry - legFtD.Height / 2));
    }

    private static void DrawPane(DrawingContext ctx, IBrush fill, IPen pen,
                                  Point c1, Point c2, Point c3, Point c4)
    {
        var geo = new StreamGeometry();
        using (var sgc = geo.Open())
        {
            sgc.BeginFigure(c1, true);
            sgc.LineTo(c2); sgc.LineTo(c3); sgc.LineTo(c4);
            sgc.EndFigure(true);
        }
        ctx.DrawGeometry(fill, pen, geo);
    }

    private static FormattedText MakeText(string s, double size, IBrush brush,
                                          Typeface? tf = null)
        => new(s, CultureInfo.InvariantCulture, FlowDirection.LeftToRight,
               tf ?? new Typeface(FontFamily.Default), size, brush);
}
