// MainWindow.cpp — 3-DOF Robot Arm Simulation Visualizer (Qt6)

#include "MainWindow.hpp"
#include "CsvReader.hpp"
#include "Widgets.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QAction>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QProcess>

namespace rl_qt {

// matplotlib tab10 colours
static const QColor kBlue      ( 31, 119, 180);
static const QColor kBlueFaded ( 31, 119, 180, 178);  // alpha ≈ 0.7, matches PyQt6 desired alpha
static const QColor kOrange    (255, 127,  14);
static const QColor kGreen     ( 44, 160,  44);
static const QColor kCrimson   (220,  20,  60);

// ── constructor ───────────────────────────────────────────────────────────
MainWindow::MainWindow(const QString& csvPath, QWidget* parent)
    : QMainWindow(parent), csvPath_(csvPath)
{
    setWindowTitle(QStringLiteral("3-DOF Robot Arm — Simulation Visualizer"));
    resize(1100, 780);
    buildUi();

    if (!csvPath_.isEmpty())
        loadAndPlot(csvPath_);
    else
        statusLbl_->setText(QStringLiteral("No data loaded."));
}

// ── UI construction ───────────────────────────────────────────────────────
void MainWindow::buildUi()
{
    // Toolbar
    auto* tb = addToolBar(QStringLiteral("Main"));
    tb->setMovable(false);

    auto makeAct = [&](const char* label) {
        auto* a = new QAction(QString::fromLatin1(label), this);
        tb->addAction(a);
        return a;
    };
    auto* actOpen    = makeAct("Open CSV");
    auto* actRun     = makeAct("Run Simulation");
    tb->addSeparator();
    auto* actPlotAll = makeAct("Plot All");
    auto* actPlay    = makeAct("Play 3-D Animation");
    auto* actStop    = makeAct("Stop Animation");

    connect(actOpen,    &QAction::triggered, this, &MainWindow::onOpenCsv);
    connect(actRun,     &QAction::triggered, this, &MainWindow::onRunSim);
    connect(actPlotAll, &QAction::triggered, this, [this]() {
        if (data_.valid) populatePlots(data_);
    });
    connect(actPlay, &QAction::triggered, this, [this]() { animWidget_->play(); });
    connect(actStop, &QAction::triggered, this, [this]() { animWidget_->stop(); });

    // Tab widget
    tabs_ = new QTabWidget;

    // ── Tab 1: Joint Angles ──────────────────────────────────────────────
    {
        auto* w    = new QWidget;
        auto* vlay = new QVBoxLayout(w);
        vlay->setSpacing(2);
        vlay->setContentsMargins(4, 4, 4, 4);

        // Panel-level title matching matplotlib suptitle
        auto* titleLbl = new QLabel(QStringLiteral("Joint Angles: Actual vs Desired"));
        QFont tf = titleLbl->font();
        tf.setBold(true);
        tf.setPointSizeF(10.0);
        titleLbl->setFont(tf);
        titleLbl->setAlignment(Qt::AlignCenter);
        vlay->addWidget(titleLbl);

        for (int i = 0; i < 3; ++i) {
            angPlot_[i] = new LinePlot;
            angPlot_[i]->setYLabel(QStringLiteral("rad"));
            angPlot_[i]->setLegendColumns(2);
            if (i == 2)
                angPlot_[i]->setXLabel(QStringLiteral("time [s]"));
            vlay->addWidget(angPlot_[i], 1);
        }
        tabs_->addTab(w, QStringLiteral("Joint Angles"));
    }

    // ── Tab 2: Torques ───────────────────────────────────────────────────
    {
        torPlot_ = new LinePlot;
        torPlot_->setTitle(QStringLiteral("Joint Torques"));
        torPlot_->setXLabel(QStringLiteral("time [s]"));
        torPlot_->setYLabel(QStringLiteral("torque [N·m]"));

        auto* w    = new QWidget;
        auto* vlay = new QVBoxLayout(w);
        vlay->setContentsMargins(4, 4, 4, 4);
        vlay->addWidget(torPlot_);
        tabs_->addTab(w, QStringLiteral("Torques"));
    }

    // ── Tab 3: Errors ────────────────────────────────────────────────────
    {
        auto* w    = new QWidget;
        auto* vlay = new QVBoxLayout(w);
        vlay->setSpacing(2);
        vlay->setContentsMargins(4, 4, 4, 4);

        auto* titleLbl = new QLabel(QStringLiteral("Tracking Errors"));
        QFont tf = titleLbl->font();
        tf.setBold(true);
        tf.setPointSizeF(10.0);
        titleLbl->setFont(tf);
        titleLbl->setAlignment(Qt::AlignCenter);
        vlay->addWidget(titleLbl);

        errAngPlot_  = new LinePlot;
        errAngPlot_->setYLabel(QStringLiteral("error [deg]"));

        errHandPlot_ = new LinePlot;
        errHandPlot_->setXLabel(QStringLiteral("time [s]"));
        errHandPlot_->setYLabel(QStringLiteral("hand position error [mm]"));

        vlay->addWidget(errAngPlot_,  1);
        vlay->addWidget(errHandPlot_, 1);
        tabs_->addTab(w, QStringLiteral("Errors"));
    }

    // Animation panel (right side of splitter)
    animWidget_ = new ArmAnimWidget;

    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(tabs_);
    splitter->addWidget(animWidget_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);

    // Status bar
    statusLbl_ = new QLabel(QStringLiteral("No data loaded."));
    statusBar()->addWidget(statusLbl_);
}

// ── CSV loading ───────────────────────────────────────────────────────────
void MainWindow::onOpenCsv()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("Open simulation results CSV"),
        QFileInfo(csvPath_).absolutePath(),
        QStringLiteral("CSV files (*.csv);;All files (*.*)"));
    if (!path.isEmpty())
        loadAndPlot(path);
}

void MainWindow::onRunSim()
{
    // Find robot_sim.exe by searching from known locations
    const QDir root = QDir(QCoreApplication::applicationDirPath());

    QStringList candidates;
    for (int up = 0; up <= 5; ++up) {
        QDir d = root;
        for (int j = 0; j < up; ++j) if (!d.cdUp()) break;
        candidates << d.filePath("build/Release/robot_sim.exe")
                   << d.filePath("build/robot_sim.exe");
    }

    QString simExe;
    for (const auto& c : candidates) {
        if (QFile::exists(c)) { simExe = c; break; }
    }

    if (simExe.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Binary not found"),
            QStringLiteral("Could not find robot_sim binary.\n"
                           "Build with:\n  cmake -B build -S .\n  cmake --build build"));
        return;
    }

    statusLbl_->setText(QStringLiteral("Running simulation…"));
    repaint();

    QProcess proc;
    proc.setProgram(simExe);
    const QString outDir = QFileInfo(simExe).dir().absoluteFilePath("../../output");
    proc.setArguments({QDir::cleanPath(outDir)});
    proc.start();
    if (!proc.waitForFinished(120000) || proc.exitCode() != 0) {
        QMessageBox::critical(this, QStringLiteral("Simulation error"),
            proc.readAllStandardError());
        statusLbl_->setText(QStringLiteral("Simulation failed."));
        return;
    }

    // Find the CSV output
    const QString csv = QDir::cleanPath(outDir + "/sim_results.csv");
    if (QFile::exists(csv))
        loadAndPlot(csv);
    else
        statusLbl_->setText(QStringLiteral("CSV file not found after simulation: ") + csv);
}

void MainWindow::loadAndPlot(const QString& path)
{
    const SimData d = readSimCsv(path);
    if (!d.valid) {
        statusLbl_->setText(QStringLiteral("Load error: ") + d.error);
        QMessageBox::critical(this, QStringLiteral("Load error"), d.error);
        return;
    }
    csvPath_ = path;
    data_    = d;
    populatePlots(d);
    statusLbl_->setText(
        QStringLiteral("Loaded: %1  (%2 rows)").arg(path).arg(d.count()));
}

// ── plot population ───────────────────────────────────────────────────────
void MainWindow::populatePlots(const SimData& d)
{
    // ── Joint Angles (actual + desired) ──────────────────────────────────
    // All joints: orange solid = actual, blue dashed = desired (matches good.png)
    static const char* const kAngNames[] = {
        "q1 (shoulder yaw)", "q2 (shoulder pitch)", "q3 (elbow)"
    };
    const QVector<double>* actuals[3] = { &d.q1,  &d.q2,  &d.q3  };
    const QVector<double>* desired[3] = { &d.qd1, &d.qd2, &d.qd3 };

    for (int i = 0; i < 3; ++i) {
        angPlot_[i]->clearSeries();
        // Desired drawn first (behind), then actual on top — same as matplotlib
        angPlot_[i]->addSeries(PlotSeries{
            QString::fromLatin1(kAngNames[i]) + QStringLiteral(" desired"),
            kBlueFaded, true,  d.t, *desired[i]
        });
        angPlot_[i]->addSeries(PlotSeries{
            QString::fromLatin1(kAngNames[i]) + QStringLiteral(" actual"),
            kOrange, false, d.t, *actuals[i]
        });
    }

    // ── Torques ───────────────────────────────────────────────────────────
    torPlot_->clearSeries();
    torPlot_->addSeries(PlotSeries{ QStringLiteral("τ1"), kBlue,   false, d.t, d.tau1 });
    torPlot_->addSeries(PlotSeries{ QStringLiteral("τ2"), kOrange, false, d.t, d.tau2 });
    torPlot_->addSeries(PlotSeries{ QStringLiteral("τ3"), kGreen,  false, d.t, d.tau3 });

    // ── Tracking Errors ───────────────────────────────────────────────────
    // Joint errors: rad → deg  (label has no "[deg]" suffix — matches PyQt6 "e1"/"e2"/"e3")
    errAngPlot_->clearSeries();
    errAngPlot_->addSeries(PlotSeries{ QStringLiteral("e1"), kBlue,   false, d.t, SimData::toDeg(d.err1) });
    errAngPlot_->addSeries(PlotSeries{ QStringLiteral("e2"), kOrange, false, d.t, SimData::toDeg(d.err2) });
    errAngPlot_->addSeries(PlotSeries{ QStringLiteral("e3"), kGreen,  false, d.t, SimData::toDeg(d.err3) });

    // Hand position error (mm) — crimson, no legend, vertical reference at t=2 s
    const QVector<double> handErr = d.handError();
    errHandPlot_->clearSeries();
    errHandPlot_->clearVLines();
    errHandPlot_->addSeries(PlotSeries{ QString(), kCrimson, false, d.t, handErr });
    errHandPlot_->setVLine(2.0, QColor(128, 128, 128), true);

    // Steady-state RMS for t >= 2 s
    double sumSq = 0.0; int ssCnt = 0;
    for (int i = 0; i < d.count(); ++i) {
        if (d.t[i] >= 2.0) { sumSq += handErr[i] * handErr[i]; ++ssCnt; }
    }
    const double rms = ssCnt > 0 ? std::sqrt(sumSq / ssCnt) : 0.0;
    errHandPlot_->setTitle(
        QStringLiteral("Steady-state hand RMS error: ") +
        QString::number(rms, 'f', 3) +
        QStringLiteral(" mm"));

    // Arm animation: compute elbow from joint angles (L2 = 0.5 m)
    constexpr double L2 = 0.5;
    QVector<ArmFrame> frames;
    frames.reserve(d.count());
    for (int i = 0; i < d.count(); ++i) {
        const double c1 = std::cos(d.q1[i]), s1 = std::sin(d.q1[i]);
        const double c2 = std::cos(d.q2[i]), s2 = std::sin(d.q2[i]);
        frames.push_back({
            L2 * c1 * c2, L2 * s1 * c2, -L2 * s2,
            d.hand_x[i],     d.hand_y[i],     d.hand_z[i],
            d.hand_des_x[i], d.hand_des_y[i], d.hand_des_z[i],
            d.t[i]
        });
    }
    animWidget_->setData(frames);
    animWidget_->play();
}

} // namespace rl_qt
