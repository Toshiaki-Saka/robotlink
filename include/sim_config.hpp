#pragma once
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <Eigen/Dense>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace robot {

struct ArmParams {
    double L2   = 0.5;
    double L3   = 0.4;
    double m2   = 1.0;
    double m3   = 0.7;
    double grav = 9.81;
};

struct SimConfig {
    double t_end = 8.0;
    double dt    = 0.005;
};

struct CtrlGains {
    Eigen::Matrix3d Kp = 80.0 * Eigen::Matrix3d::Identity();
    Eigen::Matrix3d Kd = 18.0 * Eigen::Matrix3d::Identity();
};

// Desired trajectory: q_d_i(t) = offset_i + A_i * sin(w_i * t + phi_i)
struct TrajectoryParams {
    Eigen::Vector3d A   = (Eigen::Vector3d() << 0.6, 0.5, 0.7).finished();
    Eigen::Vector3d w   = (Eigen::Vector3d() << 0.8, 1.0, 1.2).finished();
    Eigen::Vector3d phi = (Eigen::Vector3d() << 0.0, M_PI / 4.0, M_PI / 2.0).finished();
    Eigen::Vector3d off = (Eigen::Vector3d() << 0.0, -0.3, 0.8).finished();
};

struct TrajState {
    Eigen::Vector3d q;
    Eigen::Vector3d dq;
    Eigen::Vector3d ddq;
};

inline TrajState desired_trajectory(double t, const TrajectoryParams& tp) {
    TrajState s;
    for (int i = 0; i < 3; ++i) {
        s.q[i]   = tp.off[i] + tp.A[i] * std::sin(tp.w[i] * t + tp.phi[i]);
        s.dq[i]  = tp.A[i] * tp.w[i] * std::cos(tp.w[i] * t + tp.phi[i]);
        s.ddq[i] = -tp.A[i] * tp.w[i] * tp.w[i] * std::sin(tp.w[i] * t + tp.phi[i]);
    }
    return s;
}

} // namespace robot
