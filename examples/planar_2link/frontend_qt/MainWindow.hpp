// MainWindow.hpp — Qt6 GUI for the two-link manipulator demo.

#ifndef TLM_QT_MAINWINDOW_HPP
#define TLM_QT_MAINWINDOW_HPP

#include "tlm_core.h"

#include <QMainWindow>

class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSlider;

namespace tlm_qt {

class ArmPlot;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onConfigChanged();
    void onSliderChanged();
    void onResetClicked();
    void onSaveClicked();

private:
    void buildUi();
    void resetToDefaults();
    TlmRobotConfig collectConfig() const;
    void refresh();

    static void setSliderFromAngle(QSlider* sl, double angle, double lo, double hi);
    static double sliderAngle(const QSlider* sl, double lo, double hi);

    // Parameters
    QDoubleSpinBox* edL1_     = nullptr;
    QDoubleSpinBox* edL2_     = nullptr;
    QDoubleSpinBox* edT1Min_  = nullptr;
    QDoubleSpinBox* edT1Max_  = nullptr;
    QDoubleSpinBox* edT2Min_  = nullptr;
    QDoubleSpinBox* edT2Max_  = nullptr;
    QSlider*        slT1_     = nullptr;
    QSlider*        slT2_     = nullptr;
    QLabel*         lblT1_    = nullptr;
    QLabel*         lblT2_    = nullptr;
    QLabel*         lblXend_  = nullptr;
    QLabel*         lblYend_  = nullptr;
    QPushButton*    resetBtn_ = nullptr;
    QPushButton*    saveBtn_  = nullptr;
    ArmPlot*        plot_     = nullptr;
    QLabel*         statusLbl_ = nullptr;

    TlmRobotConfig cfg_{};
};

}  // namespace tlm_qt

#endif
