#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <cmath>
#include <Eigen/Dense>

#include "sim_config.hpp"
#include "robot_arm.hpp"
#include "controller.hpp"
#include "rk4.hpp"

#ifndef OUTPUT_DIR
#define OUTPUT_DIR "output"
#endif

namespace fs = std::filesystem;
using Vec6 = Eigen::Matrix<double, 6, 1>;

int main(int argc, char* argv[]) {
    const std::string out_dir  = (argc > 1) ? argv[1] : OUTPUT_DIR;
    const std::string csv_path = out_dir + "/sim_results.csv";

    fs::create_directories(out_dir);

    robot::ArmParams       arm_p;
    robot::SimConfig       sim;
    robot::CtrlGains       gains;
    robot::TrajectoryParams traj;
    robot::RobotArm        arm(arm_p);
    robot::ComputedTorqueController ctrl(arm, gains, traj);

    const int N = static_cast<int>(sim.t_end / sim.dt) + 1;

    // State: [q1, q2, q3, dq1, dq2, dq3]
    Vec6 state;
    state << 0.2, -0.1, 0.3, 0.0, 0.0, 0.0;

    std::ofstream csv(csv_path);
    if (!csv) {
        std::cerr << "Cannot open: " << csv_path << '\n';
        return 1;
    }

    // Header
    csv << "t,"
        << "q1,q2,q3,dq1,dq2,dq3,"
        << "qd1,qd2,qd3,"
        << "tau1,tau2,tau3,"
        << "err1,err2,err3,"
        << "hand_x,hand_y,hand_z,"
        << "hand_des_x,hand_des_y,hand_des_z\n";

    auto deriv = [&](const Vec6& s, double t) -> Vec6 {
        const Eigen::Vector3d q  = s.head<3>();
        const Eigen::Vector3d dq = s.tail<3>();
        const Eigen::Vector3d tau = ctrl.compute(q, dq, t);
        const Eigen::Vector3d ddq = arm.forward_dynamics(q, dq, tau);
        Vec6 ds;
        ds.head<3>() = dq;
        ds.tail<3>() = ddq;
        return ds;
    };

    std::cout << "Simulating " << N << " steps (dt=" << sim.dt << " s)...\n";

    for (int i = 0; i < N; ++i) {
        const double t = i * sim.dt;
        const Eigen::Vector3d q  = state.head<3>();
        const Eigen::Vector3d dq = state.tail<3>();

        const Eigen::Vector3d tau = ctrl.compute(q, dq, t);
        auto ref = robot::desired_trajectory(t, traj);
        const Eigen::Vector3d err = ref.q - q;

        const Eigen::Vector3d hand     = arm.fk_hand(q);
        const Eigen::Vector3d hand_des = arm.fk_hand(ref.q);

        csv << t << ','
            << q[0]  << ',' << q[1]  << ',' << q[2]  << ','
            << dq[0] << ',' << dq[1] << ',' << dq[2] << ','
            << ref.q[0] << ',' << ref.q[1] << ',' << ref.q[2] << ','
            << tau[0] << ',' << tau[1] << ',' << tau[2] << ','
            << err[0] << ',' << err[1] << ',' << err[2] << ','
            << hand[0]     << ',' << hand[1]     << ',' << hand[2]     << ','
            << hand_des[0] << ',' << hand_des[1] << ',' << hand_des[2] << '\n';

        state = robot::rk4_step(state, t, sim.dt, deriv);

        if (i % 400 == 0)
            std::cout << "  t = " << t << " s  |  q = ["
                      << q[0] << ", " << q[1] << ", " << q[2] << "]\n";
    }

    csv.close();
    std::cout << "Done. Results written to: " << csv_path << '\n';
    return 0;
}
