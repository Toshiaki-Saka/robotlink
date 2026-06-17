// Widgets.hpp — equal-aspect 2D plot for the manipulator + workspace.

#ifndef TLM_QT_WIDGETS_HPP
#define TLM_QT_WIDGETS_HPP

#include <QColor>
#include <QPointF>
#include <QVector>
#include <QWidget>

namespace tlm_qt {

// Equal-aspect 2D plot. Renders:
//   - two sets of workspace boundary polylines (red and green),
//   - the manipulator (3-point polyline + joint markers),
//   - the end-effector dot,
//   - tick labels and a legend.
class ArmPlot : public QWidget {
    Q_OBJECT
public:
    explicit ArmPlot(QWidget* parent = nullptr);

    struct Polyline {
        QColor          color;
        QVector<double> xs;
        QVector<double> ys;
    };

    // Pose: 3 points (base, joint2, end). All coordinates in metres.
    void setData(const QVector<Polyline>& redBoundary,
                 const QVector<Polyline>& greenBoundary,
                 double bx, double by,
                 double j2x, double j2y,
                 double ex, double ey,
                 double extentRadius);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QVector<Polyline> red_, green_;
    double bx_ = 0, by_ = 0, j2x_ = 1, j2y_ = 0, ex_ = 2, ey_ = 0;
    double extent_ = 2.2;
};

}  // namespace tlm_qt

#endif
