// Widgets.cpp — ArmPlot.

#include "Widgets.hpp"

#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace tlm_qt {

namespace {

double niceStep(double range, int target_ticks) {
    if (range <= 0) return 1.0;
    const double raw = range / std::max(1, target_ticks);
    const double exp = std::pow(10.0, std::floor(std::log10(raw)));
    const double n   = raw / exp;
    double mult;
    if      (n < 1.5) mult = 1.0;
    else if (n < 3.5) mult = 2.0;
    else if (n < 7.5) mult = 5.0;
    else              mult = 10.0;
    return mult * exp;
}

}  // namespace

ArmPlot::ArmPlot(QWidget* parent) : QWidget(parent) {
    setMinimumSize(480, 480);
}

void ArmPlot::setData(const QVector<Polyline>& red,
                      const QVector<Polyline>& green,
                      double bx, double by, double j2x, double j2y,
                      double ex, double ey, double extent)
{
    red_ = red; green_ = green;
    bx_ = bx; by_ = by; j2x_ = j2x; j2y_ = j2y; ex_ = ex; ey_ = ey;
    extent_ = extent > 0 ? extent : 1.0;
    update();
}

void ArmPlot::paintEvent(QPaintEvent*) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);
    g.fillRect(rect(), Qt::white);

    const int marginL = 60;
    const int marginR = 16;
    const int marginT = 32;
    const int marginB = 36;

    const double xMin = -extent_, xMax = extent_;
    const double yMin = -extent_, yMax = extent_;
    const double dx = xMax - xMin;
    const double dy = yMax - yMin;
    const double availW = width()  - marginL - marginR;
    const double availH = height() - marginT - marginB;
    if (availW <= 0 || availH <= 0) return;

    // Equal-aspect mapping.
    const double scaleX = availW / dx;
    const double scaleY = availH / dy;
    const double scale  = std::min(scaleX, scaleY);
    const double usedW  = dx * scale;
    const double usedH  = dy * scale;
    const double pxL    = marginL + (availW - usedW) / 2.0;
    const double pxR    = pxL + usedW;
    const double pxT    = marginT + (availH - usedH) / 2.0;
    const double pxB    = pxT + usedH;

    auto mapX = [&](double v) { return pxL + (v - xMin) / dx * usedW; };
    auto mapY = [&](double v) { return pxB - (v - yMin) / dy * usedH; };

    // Grid.
    const double stepX = niceStep(dx, 8);
    const double stepY = niceStep(dy, 8);
    g.setPen(QPen(QColor(230, 230, 230), 1.0));
    for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax + 1e-12; xv += stepX) {
        const double px = mapX(xv);
        g.drawLine(QPointF(px, pxT), QPointF(px, pxB));
    }
    for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax + 1e-12; yv += stepY) {
        const double py = mapY(yv);
        g.drawLine(QPointF(pxL, py), QPointF(pxR, py));
    }
    g.setPen(QPen(Qt::black, 1.0));
    g.drawRect(QRectF(pxL, pxT, usedW, usedH));

    // Tick labels.
    {
        QFont f = g.font(); f.setPointSizeF(8.0); g.setFont(f);
        QFontMetrics fm(f);
        const double xEps = (xMax - xMin) * 1e-9;
        const double yEps = (yMax - yMin) * 1e-9;
        for (double xv = std::ceil(xMin / stepX) * stepX; xv <= xMax + 1e-12; xv += stepX) {
            double v = (std::fabs(xv) < xEps) ? 0.0 : xv;
            const QString s = QString::number(v, 'g', 3);
            const double px = mapX(xv);
            g.drawText(QPointF(px - fm.horizontalAdvance(s) / 2.0,
                               pxB + fm.ascent() + 2), s);
        }
        for (double yv = std::ceil(yMin / stepY) * stepY; yv <= yMax + 1e-12; yv += stepY) {
            double v = (std::fabs(yv) < yEps) ? 0.0 : yv;
            const QString s = QString::number(v, 'g', 3);
            const double py = mapY(yv);
            g.drawText(QPointF(pxL - fm.horizontalAdvance(s) - 4,
                               py + fm.ascent() / 2 - 2), s);
        }
    }

    // Title + axis labels.
    {
        QFont f = g.font(); f.setBold(true); f.setPointSizeF(10.5); g.setFont(f);
        g.drawText(QRectF(0, 6, width(), 18), Qt::AlignCenter,
                   QStringLiteral("Two-Link Manipulator and Workspace"));
    }
    g.drawText(QRectF(0, height() - 16, width(), 14),
               Qt::AlignCenter, QStringLiteral("X"));
    g.save();
    g.translate(14, (pxT + pxB) / 2.0); g.rotate(-90);
    g.drawText(QRectF(-60, -8, 120, 16), Qt::AlignCenter, QStringLiteral("Y"));
    g.restore();

    // Workspace polylines.
    auto drawPolylines = [&](const QVector<Polyline>& lines) {
        for (const auto& pl : lines) {
            const int n = std::min(pl.xs.size(), pl.ys.size());
            if (n < 2) continue;
            QPainterPath p;
            p.moveTo(mapX(pl.xs[0]), mapY(pl.ys[0]));
            for (int i = 1; i < n; ++i)
                p.lineTo(mapX(pl.xs[i]), mapY(pl.ys[i]));
            g.setPen(QPen(pl.color, 1.5));
            g.drawPath(p);
        }
    };
    drawPolylines(red_);
    drawPolylines(green_);

    // Manipulator (thick blue polyline + node markers).
    {
        QPen pen(QColor(31, 119, 180), 3.0);
        pen.setCapStyle(Qt::RoundCap); pen.setJoinStyle(Qt::RoundJoin);
        g.setPen(pen);
        const QPointF p0(mapX(bx_),  mapY(by_));
        const QPointF p1(mapX(j2x_), mapY(j2y_));
        const QPointF p2(mapX(ex_),  mapY(ey_));
        g.drawLine(p0, p1);
        g.drawLine(p1, p2);
        // Node markers (filled circles).
        g.setBrush(QColor(31, 119, 180));
        g.setPen(QPen(QColor(31, 119, 180), 1.0));
        g.drawEllipse(p0, 5.0, 5.0);
        g.drawEllipse(p1, 5.0, 5.0);
        // End-effector dot (slightly larger, dark blue, edge).
        g.setBrush(QColor(0, 0, 180));
        g.setPen(QPen(Qt::black, 0.6));
        g.drawEllipse(p2, 6.0, 6.0);
    }

    // Legend (top-right).
    {
        QFont f = g.font(); f.setBold(false); f.setPointSizeF(8.5); g.setFont(f);
        QFontMetrics fm(f);
        const QStringList items = {
            QStringLiteral("Workspace boundary (-theta2)"),
            QStringLiteral("Workspace boundary (+theta2)"),
            QStringLiteral("Manipulator"),
            QStringLiteral("End Effector"),
        };
        int lw = 0;
        for (const auto& s : items) lw = std::max(lw, fm.horizontalAdvance(s));
        const int boxW = lw + 38;
        const int boxH = 4 + 14 * items.size();
        const QRectF box(pxR - boxW - 6, pxT + 6, boxW, boxH);
        g.setBrush(QColor(255, 255, 255, 220));
        g.setPen(QPen(QColor(180, 180, 180), 0.8));
        g.drawRect(box);

        int yy = static_cast<int>(box.top()) + 12;
        g.setPen(QPen(QColor(220, 50, 47), 2.0));
        g.drawLine(QPointF(box.left() + 6, yy), QPointF(box.left() + 26, yy));
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[0]);
        yy += 14;
        g.setPen(QPen(QColor(60, 160, 60), 2.0));
        g.drawLine(QPointF(box.left() + 6, yy), QPointF(box.left() + 26, yy));
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[1]);
        yy += 14;
        g.setPen(QPen(QColor(31, 119, 180), 3.0));
        g.drawLine(QPointF(box.left() + 6, yy), QPointF(box.left() + 26, yy));
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[2]);
        yy += 14;
        g.setBrush(QColor(0, 0, 180));
        g.setPen(QPen(Qt::black, 0.6));
        g.drawEllipse(QPointF((box.left() + 6 + box.left() + 26) / 2.0, yy), 4.0, 4.0);
        g.setPen(Qt::black); g.drawText(QPointF(box.left() + 30, yy + 4), items[3]);
    }
}

}  // namespace tlm_qt
