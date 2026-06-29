// MainWindow.cpp — Qt6 GUI for the two-link manipulator demo.

#include "MainWindow.hpp"
#include "Widgets.hpp"
#include "tlm_core.h"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <vector>

namespace tlm_qt {

namespace {

constexpr int kTicks = 1000;
constexpr double kPi = 3.14159265358979323846;

QDoubleSpinBox* makeDbl(double lo, double hi, double val, double step,
                        int decimals)
{
    auto* b = new QDoubleSpinBox;
    b->setRange(lo, hi); b->setSingleStep(step); b->setDecimals(decimals);
    b->setValue(val); b->setMinimumWidth(110);
    return b;
}

// Copy a std::vector<double> into a QVector<double> for the painter.
QVector<double> toQ(const std::vector<double>& v) {
    QVector<double> out; out.reserve(static_cast<int>(v.size()));
    for (double x : v) out.push_back(x);
    return out;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Two-link manipulator (Qt6) — %1")
                       .arg(tlm_core_version()));
    resize(1180, 760);
    tlm_core_default_config(&cfg_);
    buildUi();
    refresh();
}

void MainWindow::buildUi() {
    auto* central = new QWidget; setCentralWidget(central);

    // ===== left: parameters =====
    auto* leftW = new QWidget;
    auto* ll = new QVBoxLayout(leftW);
    ll->setContentsMargins(6, 6, 6, 6); ll->setSpacing(6);

    auto* links = new QGroupBox(QStringLiteral("Link lengths"));
    {
        auto* f = new QFormLayout(links);
        edL1_ = makeDbl(0.001, 100.0, cfg_.l1, 0.05, 4);
        edL2_ = makeDbl(0.001, 100.0, cfg_.l2, 0.05, 4);
        f->addRow(QStringLiteral("l1 [m]"), edL1_);
        f->addRow(QStringLiteral("l2 [m]"), edL2_);
    }
    ll->addWidget(links);

    auto* lim = new QGroupBox(QStringLiteral("Joint-angle limits [rad]"));
    {
        auto* f = new QFormLayout(lim);
        edT1Min_ = makeDbl(-kPi, kPi, cfg_.theta1_min, 0.05, 4);
        edT1Max_ = makeDbl(-kPi, kPi, cfg_.theta1_max, 0.05, 4);
        edT2Min_ = makeDbl(-kPi, kPi, cfg_.theta2_min, 0.05, 4);
        edT2Max_ = makeDbl(-kPi, kPi, cfg_.theta2_max, 0.05, 4);
        f->addRow(QStringLiteral("theta1_min"), edT1Min_);
        f->addRow(QStringLiteral("theta1_max"), edT1Max_);
        f->addRow(QStringLiteral("theta2_min"), edT2Min_);
        f->addRow(QStringLiteral("theta2_max"), edT2Max_);
    }
    ll->addWidget(lim);

    auto* ang = new QGroupBox(QStringLiteral("Joint angles"));
    {
        auto* v = new QVBoxLayout(ang); v->setSpacing(2);
        v->addWidget(new QLabel(QStringLiteral("theta1 [rad]")));
        auto* r1 = new QHBoxLayout;
        slT1_ = new QSlider(Qt::Horizontal);
        slT1_->setMinimum(0); slT1_->setMaximum(kTicks);
        lblT1_ = new QLabel(QStringLiteral("+0.000"));
        lblT1_->setMinimumWidth(50);
        lblT1_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        r1->addWidget(slT1_, 1); r1->addWidget(lblT1_);
        v->addLayout(r1);

        v->addWidget(new QLabel(QStringLiteral("theta2 [rad]")));
        auto* r2 = new QHBoxLayout;
        slT2_ = new QSlider(Qt::Horizontal);
        slT2_->setMinimum(0); slT2_->setMaximum(kTicks);
        lblT2_ = new QLabel(QStringLiteral("+0.000"));
        lblT2_->setMinimumWidth(50);
        lblT2_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        r2->addWidget(slT2_, 1); r2->addWidget(lblT2_);
        v->addLayout(r2);
    }
    ll->addWidget(ang);
    setSliderFromAngle(slT1_, 0.0, cfg_.theta1_min, cfg_.theta1_max);
    setSliderFromAngle(slT2_, 0.0, cfg_.theta2_min, cfg_.theta2_max);

    auto* out = new QGroupBox(QStringLiteral("End effector"));
    {
        auto* f = new QFormLayout(out);
        lblXend_ = new QLabel(QStringLiteral("+0.000"));
        lblYend_ = new QLabel(QStringLiteral("+0.000"));
        lblXend_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        lblYend_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        f->addRow(QStringLiteral("x_end"), lblXend_);
        f->addRow(QStringLiteral("y_end"), lblYend_);
    }
    ll->addWidget(out);

    auto* btns = new QHBoxLayout;
    resetBtn_ = new QPushButton(QStringLiteral("Reset"));
    saveBtn_  = new QPushButton(QStringLiteral("Save PNG…"));
    btns->addWidget(resetBtn_); btns->addWidget(saveBtn_);
    ll->addLayout(btns);
    ll->addStretch(1);

    // ===== right: plot =====
    plot_ = new ArmPlot;

    // ===== assemble =====
    auto* split = new QSplitter(Qt::Horizontal);
    split->addWidget(leftW); split->addWidget(plot_);
    split->setSizes({340, 840});
    split->setStretchFactor(0, 0); split->setStretchFactor(1, 1);

    auto* outer = new QVBoxLayout(central); outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(split, 1);

    statusLbl_ = new QLabel(QStringLiteral("Ready"));
    statusLbl_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    statusBar()->addWidget(statusLbl_);

    // Wire up.
    for (auto* w : {edL1_, edL2_, edT1Min_, edT1Max_, edT2Min_, edT2Max_})
        connect(w, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &MainWindow::onConfigChanged);
    connect(slT1_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(slT2_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(resetBtn_, &QPushButton::clicked, this, &MainWindow::onResetClicked);
    connect(saveBtn_,  &QPushButton::clicked, this, &MainWindow::onSaveClicked);
}

void MainWindow::setSliderFromAngle(QSlider* sl, double angle,
                                     double lo, double hi)
{
    if (hi <= lo) { sl->setValue(0); return; }
    double f = (angle - lo) / (hi - lo);
    f = std::max(0.0, std::min(1.0, f));
    sl->setValue(static_cast<int>(std::round(f * kTicks)));
}
double MainWindow::sliderAngle(const QSlider* sl, double lo, double hi) {
    const double f = static_cast<double>(sl->value()) / kTicks;
    return lo + f * (hi - lo);
}

TlmRobotConfig MainWindow::collectConfig() const {
    TlmRobotConfig cfg;
    tlm_core_default_config(&cfg);
    cfg.l1 = edL1_->value(); cfg.l2 = edL2_->value();
    cfg.theta1_min = edT1Min_->value(); cfg.theta1_max = edT1Max_->value();
    cfg.theta2_min = edT2Min_->value(); cfg.theta2_max = edT2Max_->value();
    return cfg;
}

void MainWindow::onConfigChanged() {
    // Preserve the *displayed* joint angles across a config change so the
    // arm doesn't snap to a new pose when the user tweaks a limit.
    const double oldT1 = sliderAngle(slT1_, cfg_.theta1_min, cfg_.theta1_max);
    const double oldT2 = sliderAngle(slT2_, cfg_.theta2_min, cfg_.theta2_max);
    cfg_ = collectConfig();
    setSliderFromAngle(slT1_, oldT1, cfg_.theta1_min, cfg_.theta1_max);
    setSliderFromAngle(slT2_, oldT2, cfg_.theta2_min, cfg_.theta2_max);
    refresh();
}

void MainWindow::onSliderChanged() { refresh(); }

void MainWindow::onResetClicked() {
    TlmRobotConfig d;
    tlm_core_default_config(&d);
    edL1_->setValue(d.l1); edL2_->setValue(d.l2);
    edT1Min_->setValue(d.theta1_min); edT1Max_->setValue(d.theta1_max);
    edT2Min_->setValue(d.theta2_min); edT2Max_->setValue(d.theta2_max);
    cfg_ = d;
    setSliderFromAngle(slT1_, 0.0, d.theta1_min, d.theta1_max);
    setSliderFromAngle(slT2_, 0.0, d.theta2_min, d.theta2_max);
    refresh();
}

void MainWindow::onSaveClicked() {
    const QString path = QFileDialog::getSaveFileName(
        this, QStringLiteral("Save plot"),
        QStringLiteral("two_link_workspace.png"),
        QStringLiteral("PNG (*.png);;All files (*)"));
    if (path.isEmpty()) return;
    plot_->grab().save(path);
    statusBar()->showMessage(QStringLiteral("Saved: %1").arg(path), 5000);
}

void MainWindow::refresh() {
    // Read current angles.
    cfg_ = collectConfig();
    const double t1 = sliderAngle(slT1_, cfg_.theta1_min, cfg_.theta1_max);
    const double t2 = sliderAngle(slT2_, cfg_.theta2_min, cfg_.theta2_max);

    TlmPose p{};
    if (!tlm_core_forward_kinematics(&cfg_, t1, t2, &p)) {
        statusLbl_->setText(QStringLiteral("Bad config"));
        return;
    }

    // Compute both workspace sides.
    TlmWorkspace* minus = tlm_core_compute_workspace(&cfg_, cfg_.theta2_min, 0.0, 100);
    TlmWorkspace* plus  = tlm_core_compute_workspace(&cfg_, 0.0, cfg_.theta2_max, 100);
    if (!minus || !plus) {
        if (minus) tlm_core_free_workspace(minus);
        if (plus)  tlm_core_free_workspace(plus);
        statusLbl_->setText(QStringLiteral("Workspace failed"));
        return;
    }

    auto extract = [&](TlmWorkspace* ws, const QColor& col) -> QVector<ArmPlot::Polyline> {
        const int n = tlm_core_ws_samples_per_curve(ws);
        std::vector<double> bx(n), by(n);
        QVector<ArmPlot::Polyline> out;
        out.reserve(4);
        auto pull = [&](auto copyFnX, auto copyFnY) {
            std::vector<double> xs(n), ys(n);
            copyFnX(ws, xs.data(), n);
            copyFnY(ws, ys.data(), n);
            out.push_back(ArmPlot::Polyline{col, toQ(xs), toQ(ys)});
        };
        pull(tlm_core_ws_copy_a_x, tlm_core_ws_copy_a_y);
        pull(tlm_core_ws_copy_b_x, tlm_core_ws_copy_b_y);
        pull(tlm_core_ws_copy_c_x, tlm_core_ws_copy_c_y);
        pull(tlm_core_ws_copy_d_x, tlm_core_ws_copy_d_y);
        return out;
    };
    const QVector<ArmPlot::Polyline> red   = extract(minus, QColor(220, 50, 47));
    const QVector<ArmPlot::Polyline> green = extract(plus,  QColor(60, 160, 60));
    tlm_core_free_workspace(minus);
    tlm_core_free_workspace(plus);

    const double extent = (cfg_.l1 + cfg_.l2) * 1.1;
    plot_->setData(red, green,
                   p.base_x, p.base_y, p.joint2_x, p.joint2_y,
                   p.end_x, p.end_y, extent);

    lblT1_->setText(QString::asprintf("%+.3f", t1));
    lblT2_->setText(QString::asprintf("%+.3f", t2));
    lblXend_->setText(QString::asprintf("%+.3f", p.end_x));
    lblYend_->setText(QString::asprintf("%+.3f", p.end_y));
    statusLbl_->setText(QString::asprintf(
        "theta1 = %+.3f rad, theta2 = %+.3f rad, end = (%+.3f, %+.3f)",
        t1, t2, p.end_x, p.end_y));
}

}  // namespace tlm_qt
