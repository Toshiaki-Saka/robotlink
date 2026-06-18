// Widgets.cpp — LinePlot implementation.
//
// Rendering order (Z-order lowest to highest):
//   1. White background fill
//   2. Grid lines  ← drawn BEFORE data so they never cover curves
//   3. Plot border box
//   4. Tick labels + axis labels
//   5. Curves (clipped to plot area)
//   6. Legend     ← drawn LAST so it appears on top of everything

#include "Widgets.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <limits>

namespace rl_qt {

// ── axis range helper ─────────────────────────────────────────────────────
void LinePlot::axisRange(const QVector<double>& vs, double& lo, double& hi)
{
    lo =  std::numeric_limits<double>::infinity();
    hi = -std::numeric_limits<double>::infinity();
    for (double v : vs) {
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    if (std::isinf(lo)) { lo = -1.0; hi = 1.0; return; }
    const double span = hi - lo;
    if (span < 1e-9) { lo -= 0.5; hi += 0.5; }
    else             { const double pad = 0.08 * span; lo -= pad; hi += pad; }
}

// ── margin calculation ────────────────────────────────────────────────────
LinePlot::PlotRect LinePlot::computeMargins(const QSize& sz, bool hasTitle, bool hasXLabel)
{
    const double mL = 58.0;
    const double mR = 14.0;
    const double mT = hasTitle  ? 26.0 : 8.0;
    const double mB = hasXLabel ? 36.0 : 24.0;
    return PlotRect{
        mL, mT,
        std::max(1.0, (double)sz.width()  - mL - mR),
        std::max(1.0, (double)sz.height() - mT - mB)
    };
}

// ── constructor ───────────────────────────────────────────────────────────
LinePlot::LinePlot(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(220, 100);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

// ── paint ─────────────────────────────────────────────────────────────────
void LinePlot::paintEvent(QPaintEvent*)
{
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing, true);

    // 1. Background
    g.fillRect(rect(), Qt::white);

    const PlotRect pr = computeMargins(size(), !title_.isEmpty(), !xLabel_.isEmpty());
    const QRectF plot(pr.left, pr.top, pr.width, pr.height);

    if (series_.isEmpty()) {
        g.setPen(QColor(140, 140, 140));
        g.drawText(rect(), Qt::AlignCenter, QStringLiteral("(no data)"));
        return;
    }

    // Compute axis ranges from all series
    QVector<double> allX, allY;
    for (const auto& s : series_) { allX << s.xs; allY << s.ys; }

    double xMin, xMax, yMin, yMax;
    axisRange(allX, xMin, xMax);
    axisRange(allY, yMin, yMax);

    const double xSpan = xMax - xMin;
    const double ySpan = yMax - yMin;

    auto xPx = [&](double x) { return plot.left() + (x - xMin) / xSpan * plot.width();  };
    auto yPx = [&](double y) { return plot.top()  + (1.0 - (y - yMin) / ySpan) * plot.height(); };

    // ── 2. Grid lines ────────────────────────────────────────────────────
    // CRITICAL: draw grid BEFORE curves so they appear behind the data.
    {
        QPen gridPen(QColor(210, 210, 210), 1.0, Qt::SolidLine);
        g.setPen(gridPen);
        constexpr int N = 5;
        for (int i = 0; i <= N; ++i) {
            const double fy = (double)i / N;
            const double fx = (double)i / N;
            // Horizontal grid line
            g.drawLine(QPointF(plot.left(),  plot.top() + fy * plot.height()),
                       QPointF(plot.right(), plot.top() + fy * plot.height()));
            // Vertical grid line
            g.drawLine(QPointF(plot.left()  + fx * plot.width(), plot.top()),
                       QPointF(plot.left()  + fx * plot.width(), plot.bottom()));
        }
    }

    // ── 2b. Vertical reference lines ─────────────────────────────────────
    for (const auto& vl : vLines_) {
        if (vl.xVal < xMin || vl.xVal > xMax) continue;
        const double px = xPx(vl.xVal);
        QPen vpen(vl.color, 1.0);
        if (vl.dashed) { vpen.setStyle(Qt::DashLine); vpen.setDashPattern({5, 3}); }
        g.setPen(vpen);
        g.drawLine(QPointF(px, plot.top()), QPointF(px, plot.bottom()));
    }

    // ── 3. Plot border ───────────────────────────────────────────────────
    g.setPen(QPen(Qt::black, 1.0));
    g.drawRect(plot);

    // ── 4a. Y tick labels ────────────────────────────────────────────────
    {
        QFont f = g.font();
        f.setPointSizeF(7.5);
        g.setFont(f);
        QFontMetricsF fm(f);
        g.setPen(Qt::black);

        constexpr int N = 5;
        for (int i = 0; i <= N; ++i) {
            const double fy = (double)i / N;
            const double yv = yMax - fy * ySpan;
            const QString sy = QString::number(yv, 'g', 3);
            const double tw = fm.horizontalAdvance(sy);
            g.drawText(QPointF(plot.left() - tw - 4.0,
                               plot.top()  + fy * plot.height() + fm.ascent() / 2.0 - 1.0), sy);
        }

        // X tick labels: 6 evenly spaced
        for (int i = 0; i <= N; ++i) {
            const double fx = (double)i / N;
            const double xv = xMin + fx * xSpan;
            const QString sx = QString::number(xv, 'g', 3);
            const double tw = fm.horizontalAdvance(sx);
            g.drawText(QPointF(plot.left() + fx * plot.width() - tw / 2.0,
                               plot.bottom() + fm.ascent() + 3.0), sx);
        }
    }

    // ── 4b. Axis labels ──────────────────────────────────────────────────
    if (!xLabel_.isEmpty()) {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetricsF fm(f);
        g.setPen(Qt::black);
        g.drawText(QPointF(plot.left() + (plot.width() - fm.horizontalAdvance(xLabel_)) / 2.0,
                           plot.bottom() + 22.0), xLabel_);
    }
    if (!yLabel_.isEmpty()) {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetricsF fm(f);
        g.save();
        g.translate(11.0, plot.top() + (plot.height() + fm.horizontalAdvance(yLabel_)) / 2.0);
        g.rotate(-90.0);
        g.setPen(Qt::black);
        g.drawText(QPointF(0, 0), yLabel_);
        g.restore();
    }

    // ── 4c. Title ────────────────────────────────────────────────────────
    if (!title_.isEmpty()) {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(9.0); g.setFont(f);
        g.setPen(Qt::black);
        g.drawText(QRectF(0, 2.0, width(), pr.top - 4.0), Qt::AlignCenter, title_);
    }

    // ── 5. Curves (clipped to plot area) ─────────────────────────────────
    g.setClipRect(plot.adjusted(-1, -1, 1, 1));
    for (const auto& s : series_) {
        if (s.xs.size() < 2) continue;
        QPen pen(s.color, 1.6);
        if (s.dashed) {
            pen.setStyle(Qt::DashLine);
            pen.setDashPattern({6, 3});
        }
        g.setPen(pen);

        QPainterPath path;
        path.moveTo(xPx(s.xs[0]), yPx(s.ys[0]));
        for (int i = 1; i < s.xs.size(); ++i)
            path.lineTo(xPx(s.xs[i]), yPx(s.ys[i]));
        g.drawPath(path);
    }
    g.setClipping(false);

    // ── 6. Legend ─────────────────────────────────────────────────────────
    QVector<const PlotSeries*> labeled;
    for (const auto& s : series_)
        if (!s.label.isEmpty()) labeled.push_back(&s);

    if (!labeled.isEmpty()) {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(7.5); g.setFont(f);
        QFontMetricsF fm(f);

        const double swW  = 18.0;
        const double gap  =  4.0;
        const double lh   = fm.height() + 3.0;
        const double vpad =  4.0;
        const double hpad =  5.0;
        const int    nCols = legendColumns_;
        const int    nRows = ((int)labeled.size() + nCols - 1) / nCols;

        double maxTextW = 0.0;
        for (const auto* sp : labeled)
            maxTextW = std::max(maxTextW, fm.horizontalAdvance(sp->label));

        // colW = stride from one column's left edge to the next
        const double colW = hpad + swW + gap + maxTextW;
        const double boxW = colW * nCols + hpad;
        const double boxH = nRows * lh + vpad * 2.0;

        const double lx = plot.right() - boxW - 6.0;
        const double ly = plot.top()   + 6.0;

        g.setPen(QPen(QColor(160, 160, 160), 0.7));
        g.setBrush(QColor(255, 255, 255, 210));
        g.drawRect(QRectF(lx, ly, boxW, boxH));

        for (int k = 0; k < (int)labeled.size(); ++k) {
            const int    col  = k % nCols;
            const int    row  = k / nCols;
            const double rx   = lx + col * colW;
            const double ry   = ly + vpad + row * lh;
            const auto*  sp   = labeled[k];

            QPen swPen(sp->color, 2.0);
            if (sp->dashed) { swPen.setStyle(Qt::DashLine); swPen.setDashPattern({5, 2}); }
            g.setPen(swPen);
            g.setBrush(Qt::NoBrush);
            g.drawLine(QPointF(rx + hpad,        ry + lh / 2.0),
                       QPointF(rx + hpad + swW,  ry + lh / 2.0));

            g.setPen(Qt::black);
            g.drawText(QPointF(rx + hpad + swW + gap, ry + fm.ascent()), sp->label);
        }
    }
}

// ── ArmAnimWidget ─────────────────────────────────────────────────────────

ArmAnimWidget::ArmAnimWidget(QWidget* parent)
    : QWidget(parent)
    , timer_(new QTimer(this))
{
    setMinimumSize(300, 260);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    connect(timer_, &QTimer::timeout, this, &ArmAnimWidget::advance);
}

void ArmAnimWidget::setData(const QVector<ArmFrame>& frames)
{
    frames_ = frames;
    frame_  = 0;
    update();
}

void ArmAnimWidget::play()
{
    if (!frames_.isEmpty() && !timer_->isActive())
        timer_->start(30);
}

void ArmAnimWidget::stop()
{
    timer_->stop();
}

void ArmAnimWidget::advance()
{
    if (frames_.isEmpty()) return;
    frame_ = (frame_ + kStride) % frames_.size();
    update();
}

QPointF ArmAnimWidget::project(double x, double y, double z) const
{
    // Matches matplotlib default 3-D view: azimuth=-60°, elevation=30°.
    // matplotlib rotates the CAMERA by azim, equivalent to rotating the WORLD by +60°:
    //   R_z(+60°): x' = 0.5x − 0.866y,  y' = 0.866x + 0.5y
    // Then tilt for elev=30° (R_x(-30°) on scene):
    //   screen-right: u = x'               = 0.500x − 0.866y
    //   screen-down:  v = 0.5y' − 0.866z  = 0.433x + 0.250y − 0.866z
    const double u =  0.500 * x - 0.866 * y;
    const double v =  0.433 * x + 0.250 * y - 0.866 * z;
    const double scale = std::min(width(), height()) * 0.36;
    // Shift centre upward slightly to account for z-range asymmetry (-0.54..0.9 → centre 0.18)
    return QPointF(width()  * 0.5 + scale * u,
                   height() * 0.5 + scale * (v - 0.10));
}

void ArmAnimWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);

    // Title
    {
        QFont tf = p.font(); tf.setBold(true); tf.setPointSizeF(9.0); p.setFont(tf);
        p.setPen(Qt::black);
        p.drawText(QRectF(0, 2, width(), 20), Qt::AlignCenter | Qt::AlignTop,
                   QStringLiteral("3-DOF Arm Trajectory Tracking"));
    }

    if (frames_.isEmpty()) {
        QFont tf = p.font(); tf.setBold(false); p.setFont(tf);
        p.setPen(QColor(140, 140, 140));
        p.drawText(rect(), Qt::AlignCenter, QStringLiteral("(no data)"));
        return;
    }

    const auto& f = frames_[frame_];

    // Bounding box — mirrors PyQt6: xlim/ylim=(-0.9,0.9), zlim=(-0.54,0.9)
    const double XN = -0.9, XX = 0.9;
    const double YN = -0.9, YX = 0.9;
    const double ZN = -0.54, ZX = 0.9;

    // ── 1. Gray background panes (matplotlib Axes3D default style) ────────
    // Three panes visible from azimuth=-60°, elevation=30°:
    //   bottom (z=ZN), back wall (y=YX), left wall (x=XN) — far face from camera
    const QColor paneColor(235, 235, 235);

    auto drawPane = [&](std::initializer_list<std::array<double,3>> corners) {
        QPolygonF poly;
        for (const auto& c : corners)
            poly << project(c[0], c[1], c[2]);
        p.setPen(Qt::NoPen);
        p.setBrush(paneColor);
        p.drawPolygon(poly);
    };

    // Bottom pane  (z = ZN)
    drawPane({ {XN,YN,ZN}, {XX,YN,ZN}, {XX,YX,ZN}, {XN,YX,ZN} });
    // Back wall    (y = YX)
    drawPane({ {XN,YX,ZN}, {XX,YX,ZN}, {XX,YX,ZX}, {XN,YX,ZX} });
    // Left wall    (x = XN) — far face from camera at azimuth=-60°
    drawPane({ {XN,YN,ZN}, {XN,YX,ZN}, {XN,YX,ZX}, {XN,YN,ZX} });

    // ── 2. White grid lines on panes (spacing 0.2 → matches matplotlib auto-ticks) ──
    {
        QPen gp(Qt::white, 0.8);
        p.setPen(gp);
        p.setBrush(Qt::NoBrush);

        // Bottom pane grid (XY at z=ZN) — -0.75..0.75 step 0.25 (matches matplotlib auto-ticks)
        for (double v = -0.75; v <= 0.76; v += 0.25) {
            p.drawLine(project(v,  YN, ZN), project(v,  YX, ZN));
            p.drawLine(project(XN, v,  ZN), project(XX, v,  ZN));
        }
        // Back wall (y=YX) grid — X: -0.75..0.75 step 0.25, Z: -0.4..0.8 step 0.2
        for (double v = -0.75; v <= 0.76; v += 0.25)
            p.drawLine(project(v,  YX, ZN), project(v,  YX, ZX));
        for (double v = -0.4; v <= 0.81; v += 0.2)
            p.drawLine(project(XN, YX, v),  project(XX, YX, v));
        // Left wall (x=XN) grid — Y: -0.75..0.75 step 0.25, Z: -0.4..0.8 step 0.2
        for (double v = -0.75; v <= 0.76; v += 0.25)
            p.drawLine(project(XN, v,  ZN), project(XN, v,  ZX));
        for (double v = -0.4; v <= 0.81; v += 0.2)
            p.drawLine(project(XN, YN, v),  project(XN, YX, v));
    }

    // ── 3. Axis box edges (dark, thin) ────────────────────────────────────
    {
        QPen ep(QColor(50, 50, 50), 0.8);
        p.setPen(ep);
        p.setBrush(Qt::NoBrush);
        // Bottom rectangle
        p.drawLine(project(XN,YN,ZN), project(XX,YN,ZN));
        p.drawLine(project(XX,YN,ZN), project(XX,YX,ZN));
        p.drawLine(project(XX,YX,ZN), project(XN,YX,ZN));
        p.drawLine(project(XN,YX,ZN), project(XN,YN,ZN));
        // Top edges (back wall + left wall tops, plus front-right)
        p.drawLine(project(XN,YN,ZX), project(XN,YX,ZX));
        p.drawLine(project(XN,YX,ZX), project(XX,YX,ZX));
        p.drawLine(project(XX,YN,ZX), project(XX,YX,ZX));
        // Vertical edges (all four visible corners)
        p.drawLine(project(XN,YN,ZN), project(XN,YN,ZX));
        p.drawLine(project(XN,YX,ZN), project(XN,YX,ZX));
        p.drawLine(project(XX,YX,ZN), project(XX,YX,ZX));
        p.drawLine(project(XX,YN,ZN), project(XX,YN,ZX));  // front-right — Z spine
    }

    // ── 3b. Axis tick marks and numeric labels ────────────────────────────
    // Spine positions match matplotlib azim=-60°, elev=30° convention:
    //   X spine: back-bottom edge  (y=YX, z=ZN)  — shared by floor + back wall
    //   Y spine: left-bottom edge  (x=XN, z=ZN)  — shared by floor + left wall
    //   Z spine: front-right edge  (x=XX, y=YN)  — rightmost visible vertical
    {
        QFont af = p.font(); af.setPointSizeF(6.5); p.setFont(af);
        QFontMetricsF fm(af);
        p.setBrush(Qt::NoBrush);

        // X ticks — back-bottom edge (y=YX, z=ZN); offset outward in +y
        p.setPen(QPen(QColor(80, 80, 80), 0.8));
        for (double tv : {-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75})
            p.drawLine(project(tv, YX, ZN), project(tv, YX + 0.05, ZN));
        p.setPen(QColor(60, 60, 60));
        for (double tv : {-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75}) {
            const QString s = QString::number(tv, 'f', 2);
            const QPointF lp = project(tv, YX + 0.15, ZN - 0.04);
            p.drawText(lp + QPointF(-fm.horizontalAdvance(s) / 2.0, fm.ascent() * 0.5), s);
        }

        // Y ticks — left-bottom edge (x=XN, z=ZN); offset outward in -x
        p.setPen(QPen(QColor(80, 80, 80), 0.8));
        for (double tv : {-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75})
            p.drawLine(project(XN, tv, ZN), project(XN - 0.05, tv, ZN));
        p.setPen(QColor(60, 60, 60));
        for (double tv : {-0.75,-0.50,-0.25,0.00,0.25,0.50,0.75}) {
            const QString s = QString::number(tv, 'f', 2);
            const QPointF lp = project(XN - 0.13, tv, ZN - 0.04);
            p.drawText(lp + QPointF(-fm.horizontalAdvance(s), fm.ascent() * 0.5), s);
        }

        // Z ticks — front-right vertical edge (x=XX, y=YN); offset outward in +x / -y
        p.setPen(QPen(QColor(80, 80, 80), 0.8));
        for (double tv : {-0.4,-0.2,0.0,0.2,0.4,0.6,0.8})
            p.drawLine(project(XX, YN, tv), project(XX + 0.05, YN - 0.02, tv));
        p.setPen(QColor(60, 60, 60));
        for (double tv : {-0.4,-0.2,0.0,0.2,0.4,0.6,0.8}) {
            const QString s = QString::number(tv, 'f', 1);
            const QPointF lp = project(XX + 0.13, YN - 0.04, tv);
            p.drawText(lp + QPointF(2.0, fm.ascent() * 0.5), s);
        }
    }

    // ── 4. Axis labels X / Y / Z ──────────────────────────────────────────
    {
        QFont af = p.font(); af.setPointSizeF(8.0); p.setFont(af);
        p.setPen(QColor(50, 50, 50));
        // X: centre of back-bottom spine (y=YX), extended outward in +y
        p.drawText(project(0, YX + 0.20, ZN), QStringLiteral("X"));
        // Y: centre of left-bottom spine (x=XN), extended outward in -x
        p.drawText(project(XN - 0.20, 0, ZN), QStringLiteral("Y"));
        // Z: top of front-right vertical spine (x=XX, y=YN), extended upward
        p.drawText(project(XX, YN, ZX + 0.14), QStringLiteral("Z"));
    }

    // ── 5. Floor wireframe at z=0 ─────────────────────────────────────────
    // Matches PyQt6: ax.plot_wireframe(Xg, Yg, 0, color="lightgray", lw=0.3)
    // np.linspace(-0.9, 0.9, 5) → {-0.9, -0.45, 0, 0.45, 0.9}
    {
        p.setPen(QPen(QColor(200, 200, 200), 0.5));
        p.setBrush(Qt::NoBrush);
        for (double v = -0.9; v <= 0.91; v += 0.45) {
            p.drawLine(project(-0.9, v,   0.0), project(0.9, v,   0.0));
            p.drawLine(project(v,   -0.9, 0.0), project(v,   0.9, 0.0));
        }
    }

    // ── 6. Trajectories (up to current frame) ────────────────────────────
    if (frame_ > 0) {
        // actual hand path — tab:orange (matches PyQt6 "tab:orange")
        p.setPen(QPen(QColor(255, 127, 14), 1.2));
        p.setBrush(Qt::NoBrush);
        for (int i = 1; i <= frame_; ++i)
            p.drawLine(project(frames_[i-1].hx,  frames_[i-1].hy,  frames_[i-1].hz),
                       project(frames_[i].hx,    frames_[i].hy,    frames_[i].hz));

        // desired hand path — tab:green dashed (matches PyQt6 "tab:green")
        QPen dp(QColor(44, 160, 44), 1.2);
        dp.setStyle(Qt::DashLine);
        dp.setDashPattern({5, 2});
        p.setPen(dp);
        for (int i = 1; i <= frame_; ++i)
            p.drawLine(project(frames_[i-1].hdx, frames_[i-1].hdy, frames_[i-1].hdz),
                       project(frames_[i].hdx,   frames_[i].hdy,   frames_[i].hdz));
    }

    // ── 7. Arm links — steelblue "o-" style (matches PyQt6 lw=4, markersize=7) ──
    const QPointF ptBase  = project(0,    0,    0   );
    const QPointF ptElbow = project(f.ex, f.ey, f.ez);
    const QPointF ptHand  = project(f.hx, f.hy, f.hz);

    p.setPen(QPen(QColor(31, 119, 180), 4.0, Qt::SolidLine, Qt::RoundCap));
    p.setBrush(Qt::NoBrush);
    p.drawLine(ptBase, ptElbow);
    p.drawLine(ptElbow, ptHand);

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(31, 119, 180));
    p.drawEllipse(ptBase,  5.5, 5.5);
    p.drawEllipse(ptElbow, 5.5, 5.5);
    p.drawEllipse(ptHand,  5.5, 5.5);

    // ── 8. Time label (upper-left, matches ax.text2D(0.02, 0.95, ...)) ───
    {
        QFont tf = p.font(); tf.setBold(false); tf.setPointSizeF(8.5); p.setFont(tf);
        p.setPen(Qt::black);
        p.drawText(QPointF(8, 28), QString("t = %1 s").arg(f.t, 0, 'f', 2));
    }

    // ── 9. Legend (upper-right, matches ax.legend(loc="upper right")) ────
    {
        QFont tf = p.font(); tf.setPointSizeF(7.5); p.setFont(tf);
        QFontMetricsF fm(tf);
        const double swW = 22.0, swGap = 4.0, legPad = 6.0, rowH = 14.0;
        const double legW = legPad + swW + swGap
                          + std::max(fm.horizontalAdvance(QStringLiteral("actual hand path")),
                                     fm.horizontalAdvance(QStringLiteral("desired hand path")))
                          + legPad;
        const double legH = legPad + 2.0 * rowH + legPad;
        const double lx   = width()  - legW - 8.0;
        const double ly   = 26.0;

        p.setPen(QPen(QColor(160, 160, 160), 0.7));
        p.setBrush(QColor(255, 255, 255, 210));
        p.drawRect(QRectF(lx, ly, legW, legH));

        const double rx = lx + legPad;
        double ry = ly + legPad + rowH * 0.5;

        p.setPen(QPen(QColor(255, 127, 14), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(rx, ry), QPointF(rx + swW, ry));
        p.setPen(Qt::black);
        p.drawText(QPointF(rx + swW + swGap, ry + fm.ascent() * 0.5),
                   QStringLiteral("actual hand path"));

        ry += rowH;
        QPen dpen(QColor(44, 160, 44), 1.5);
        dpen.setStyle(Qt::DashLine);
        dpen.setDashPattern({5, 2});
        p.setPen(dpen);
        p.drawLine(QPointF(rx, ry), QPointF(rx + swW, ry));
        p.setPen(Qt::black);
        p.drawText(QPointF(rx + swW + swGap, ry + fm.ascent() * 0.5),
                   QStringLiteral("desired hand path"));
    }
}

} // namespace rl_qt
