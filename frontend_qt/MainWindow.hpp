#pragma once
// MainWindow.hpp — 3-DOF Robot Arm Simulation Visualizer (Qt6)

#include "CsvReader.hpp"
#include "Widgets.hpp"

#include <QMainWindow>
#include <QString>

class QLabel;
class QSplitter;
class QTabWidget;

namespace rl_qt {

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& csvPath = {}, QWidget* parent = nullptr);

private slots:
    void onOpenCsv();
    void onRunSim();

private:
    void buildUi();
    void loadAndPlot(const QString& path);
    void populatePlots(const SimData& d);

    QString  csvPath_;
    SimData  data_;

    // Tab 1: Joint Angles (3 plots)
    LinePlot* angPlot_[3] = {};

    // Tab 2: Torques (1 plot, 3 series)
    LinePlot* torPlot_ = nullptr;

    // Tab 3: Tracking Errors (2 plots)
    LinePlot* errAngPlot_  = nullptr;
    LinePlot* errHandPlot_ = nullptr;

    ArmAnimWidget* animWidget_ = nullptr;

    QLabel*     statusLbl_ = nullptr;
    QTabWidget* tabs_      = nullptr;
};

} // namespace rl_qt
