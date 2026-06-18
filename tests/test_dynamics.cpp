// Physics-based regression tests for the generated arm dynamics.
//
// These check mathematical properties that MUST hold regardless of how the
// SymPy derivation evolves:
//   - the mass matrix is symmetric and positive-definite,
//   - a static pose held against gravity requires exactly tau = g(q),
//   - forward dynamics inverts the dynamics consistently,
//   - the controller drives the tracking error toward zero.

#include "test_harness.hpp"

#include <Eigen/Dense>

#include "sim_config.hpp"
#include "robot_arm.hpp"
#include "controller.hpp"
#include "rk4.hpp"

using Vec3 = Eigen::Vector3d;
using Vec6 = Eigen::Matrix<double, 6, 1>;

namespace {

// A few representative joint configurations to exercise the dynamics.
const Vec3 kPoses[] = {
    Vec3{0.0, 0.0, 0.0},
    Vec3{0.2, -0.1, 0.3},
    Vec3{0.7, 0.5, -0.4},
    Vec3{-0.6, 0.9, 1.1},
};

} // namespace

TEST("mass matrix is symmetric") {
    robot::RobotArm arm{robot::ArmParams{}};
    for (const auto& q : kPoses) {
        const auto d = arm.dynamics(q, Vec3::Zero());
        for (int i = 0; i < 3; ++i)
            for (int j = i + 1; j < 3; ++j)
                CHECK_NEAR(d.M(i, j), d.M(j, i), 1e-9);
    }
}

TEST("mass matrix is positive-definite") {
    robot::RobotArm arm{robot::ArmParams{}};
    for (const auto& q : kPoses) {
        const auto d = arm.dynamics(q, Vec3::Zero());
        // Symmetric eigenvalues; all must be strictly positive.
        Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(d.M);
        CHECK(es.info() == Eigen::Success);
        for (int i = 0; i < 3; ++i)
            CHECK(es.eigenvalues()(i) > 1e-9);
    }
}

TEST("gravity term holds a static pose") {
    // At rest (dq = 0), forward dynamics with tau = g(q) must give zero
    // acceleration: the gravity torque exactly balances the pose.
    robot::RobotArm arm{robot::ArmParams{}};
    for (const auto& q : kPoses) {
        const auto d = arm.dynamics(q, Vec3::Zero());
        const Vec3 ddq = arm.forward_dynamics(q, Vec3::Zero(), d.gv);
        for (int i = 0; i < 3; ++i)
            CHECK_NEAR(ddq(i), 0.0, 1e-9);
    }
}

TEST("forward dynamics inverts the equations of motion") {
    // For an arbitrary tau, ddq = M^-1 (tau - Cdq - g) must satisfy
    // M*ddq + Cdq + g = tau.
    robot::RobotArm arm{robot::ArmParams{}};
    const Vec3 q{0.3, -0.2, 0.5};
    const Vec3 dq{0.4, 0.1, -0.3};
    const Vec3 tau{1.5, -0.7, 0.9};

    const auto d = arm.dynamics(q, dq);
    const Vec3 ddq = arm.forward_dynamics(q, dq, tau);
    const Vec3 residual = d.M * ddq + d.Cdq + d.gv - tau;
    for (int i = 0; i < 3; ++i)
        CHECK_NEAR(residual(i), 0.0, 1e-9);
}

TEST("forward kinematics: zero pose places hand on +x axis") {
    robot::RobotArm arm{robot::ArmParams{}};
    const robot::ArmParams p{};
    const Vec3 hand = arm.fk_hand(Vec3::Zero());
    // q = 0 -> both links along world +x.
    CHECK_NEAR(hand.x(), p.L2 + p.L3, 1e-12);
    CHECK_NEAR(hand.y(), 0.0, 1e-12);
    CHECK_NEAR(hand.z(), 0.0, 1e-12);
}

TEST("computed-torque controller drives tracking error to zero") {
    // Run the full closed loop and confirm the error shrinks well below its
    // initial value after a few seconds.
    robot::ArmParams        arm_p;
    robot::CtrlGains        gains;
    robot::TrajectoryParams traj;
    robot::RobotArm         arm{arm_p};
    robot::ComputedTorqueController ctrl{arm, gains, traj};

    const double dt = 0.005;
    const int    steps = 1200;  // 6 s

    Vec6 state;
    state << 0.2, -0.1, 0.3, 0.0, 0.0, 0.0;

    auto deriv = [&](const Vec6& s, double t) -> Vec6 {
        const Vec3 q  = s.head<3>();
        const Vec3 dq = s.tail<3>();
        const Vec3 tau = ctrl.compute(q, dq, t);
        const Vec3 ddq = arm.forward_dynamics(q, dq, tau);
        Vec6 ds;
        ds.head<3>() = dq;
        ds.tail<3>() = ddq;
        return ds;
    };

    auto err_norm = [&](double t, const Vec6& s) {
        const auto ref = robot::desired_trajectory(t, traj);
        return (ref.q - s.head<3>()).norm();
    };

    const double e0 = err_norm(0.0, state);
    for (int i = 0; i < steps; ++i) {
        const double t = i * dt;
        state = robot::rk4_step(state, t, dt, deriv);
    }
    const double e_final = err_norm(steps * dt, state);

    CHECK(e0 > 1e-3);                 // there really was an initial error
    CHECK(e_final < 0.05 * e0);       // controller reduced it by >20x
    CHECK(state.allFinite());         // no blow-up
}

int main() {
    return robotlink_test::run_all();
}
