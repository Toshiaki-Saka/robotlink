#pragma once
// CsvReader.hpp — reads output/sim_results.csv into SimData.

#include <QFile>
#include <QMap>
#include <QString>
#include <QTextStream>
#include <QVector>
#include <cmath>

struct SimData {
    QVector<double> t;
    QVector<double> q1, q2, q3;
    QVector<double> qd1, qd2, qd3;
    QVector<double> tau1, tau2, tau3;
    QVector<double> err1, err2, err3;
    QVector<double> hand_x,     hand_y,     hand_z;
    QVector<double> hand_des_x, hand_des_y, hand_des_z;

    bool    valid = false;
    QString error;

    int count() const { return t.size(); }

    QVector<double> handError() const {
        QVector<double> out;
        out.reserve(t.size());
        for (int i = 0; i < t.size(); ++i) {
            const double dx = hand_x[i] - hand_des_x[i];
            const double dy = hand_y[i] - hand_des_y[i];
            const double dz = hand_z[i] - hand_des_z[i];
            out.push_back(std::sqrt(dx*dx + dy*dy + dz*dz) * 1000.0); // mm
        }
        return out;
    }

    static QVector<double> toDeg(const QVector<double>& rad) {
        QVector<double> out;
        out.reserve(rad.size());
        constexpr double R = 180.0 / 3.14159265358979323846;
        for (double v : rad) out.push_back(v * R);
        return out;
    }
};

inline SimData readSimCsv(const QString& path)
{
    SimData d;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        d.error = "Cannot open: " + path;
        return d;
    }
    QTextStream s(&f);

    if (s.atEnd()) { d.error = "File is empty"; return d; }
    const QStringList rawHeaders = s.readLine().split(',');
    QMap<QString, int> col;
    for (int i = 0; i < rawHeaders.size(); ++i)
        col[rawHeaders[i].trimmed()] = i;

    auto get = [&](const QStringList& row, const QString& name) -> double {
        const auto it = col.find(name);
        if (it == col.end() || it.value() >= row.size()) return 0.0;
        bool ok;
        const double v = row[it.value()].trimmed().toDouble(&ok);
        return ok ? v : 0.0;
    };

    while (!s.atEnd()) {
        const QStringList row = s.readLine().split(',');
        if (row.isEmpty() || row[0].trimmed().isEmpty()) continue;
        d.t.push_back(get(row, "t"));
        d.q1.push_back(get(row, "q1")); d.q2.push_back(get(row, "q2")); d.q3.push_back(get(row, "q3"));
        d.qd1.push_back(get(row,"qd1")); d.qd2.push_back(get(row,"qd2")); d.qd3.push_back(get(row,"qd3"));
        d.tau1.push_back(get(row,"tau1")); d.tau2.push_back(get(row,"tau2")); d.tau3.push_back(get(row,"tau3"));
        d.err1.push_back(get(row,"err1")); d.err2.push_back(get(row,"err2")); d.err3.push_back(get(row,"err3"));
        d.hand_x.push_back(get(row,"hand_x"));
        d.hand_y.push_back(get(row,"hand_y"));
        d.hand_z.push_back(get(row,"hand_z"));
        d.hand_des_x.push_back(get(row,"hand_des_x"));
        d.hand_des_y.push_back(get(row,"hand_des_y"));
        d.hand_des_z.push_back(get(row,"hand_des_z"));
    }

    d.valid = !d.t.isEmpty();
    if (!d.valid) d.error = "No data rows found in " + path;
    return d;
}
