// main.cpp — RobotLink Qt6 Visualizer entry point.
//
// Usage:
//   robotlink_viz_qt [path/to/sim_results.csv]
//
// If no path is given, the executable walks up the directory tree looking for
// output/sim_results.csv.

#include "MainWindow.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>

static QString findDefaultCsv()
{
    QDir d(QCoreApplication::applicationDirPath());
    for (int up = 0; up <= 6; ++up) {
        const QString c = QDir::cleanPath(d.filePath("output/sim_results.csv"));
        if (QFile::exists(c)) return c;
        if (!d.cdUp()) break;
    }
    return {};
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("RobotLink Visualizer"));
    app.setOrganizationName(QStringLiteral("RobotLink"));

    QString csvPath;
    if (argc > 1)
        csvPath = QString::fromLocal8Bit(argv[1]);
    else
        csvPath = findDefaultCsv();

    rl_qt::MainWindow w(csvPath);
    w.show();
    return app.exec();
}
