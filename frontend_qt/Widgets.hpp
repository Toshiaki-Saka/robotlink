#pragma once
// Widgets.hpp — LinePlot: a time-series plot widget with grid lines and legend.
//              ArmAnimWidget: 3-D arm animation using isometric QPainter projection.
//
// Grid lines are ALWAYS drawn before the curve paths so they never obscure data.
// Legend entries are drawn in the upper-right corner of the plot area.

#include <algorithm>
#include <QColor>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

namespace rl_qt {

struct PlotSeries {
    QString         label;
    QColor          color;
    bool            dashed = false;
    QVector<double> xs;
    QVector<double> ys;
};

struct VLine {
    double xVal;
    QColor color;
    bool   dashed;
};

class LinePlot : public QWidget {
    Q_OBJECT
public:
    explicit LinePlot(QWidget* parent = nullptr);

    void setTitle         (const QString& t) { title_  = t; update(); }
    void setXLabel        (const QString& t) { xLabel_ = t; update(); }
    void setYLabel        (const QString& t) { yLabel_ = t; update(); }
    void setLegendColumns (int n)            { legendColumns_ = std::max(1, n); update(); }

    void addSeries  (const PlotSeries& s) { series_.push_back(s);  update(); }
    void clearSeries()                    { series_.clear();        update(); }

    void setVLine  (double x, QColor c = QColor(128, 128, 128), bool dash = true)
        { vLines_.push_back({x, c, dash}); update(); }
    void clearVLines() { vLines_.clear(); update(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QString title_, xLabel_, yLabel_;
    QVector<PlotSeries> series_;
    QVector<VLine>      vLines_;
    int legendColumns_ = 1;

    struct PlotRect {
        double left, top, width, height;
        double right()  const { return left + width; }
        double bottom() const { return top  + height; }
    };

    static PlotRect computeMargins(const QSize& sz, bool hasTitle, bool hasXLabel);
    static void     axisRange(const QVector<double>& vs, double& lo, double& hi);
};

// ── ArmAnimWidget ─────────────────────────────────────────────────────────

struct ArmFrame {
    double ex, ey, ez;    // elbow world position
    double hx, hy, hz;    // hand actual
    double hdx, hdy, hdz; // hand desired
    double t;
};

class ArmAnimWidget : public QWidget {
    Q_OBJECT
public:
    explicit ArmAnimWidget(QWidget* parent = nullptr);
    void setData(const QVector<ArmFrame>& frames);

public slots:
    void play();
    void stop();

protected:
    void paintEvent(QPaintEvent*) override;

private slots:
    void advance();

private:
    QPointF project(double x, double y, double z) const;

    QVector<ArmFrame> frames_;
    int               frame_ = 0;
    QTimer*           timer_;
    static constexpr int kStride = 6; // ~30 fps at dt=0.005
};

} // namespace rl_qt
