#pragma once
#include <Eigen/Dense>
#include "robot_arm.hpp"
#include "sim_config.hpp"

namespace robot {

// Computed Torque Control (inverse dynamics):
//   tau = M(q) * [ddq_d + Kd*(dq_d - dq) + Kp*(q_d - q)] + C(q,dq)*dq + g(q)
// Closed-loop error dynamics: e'' + Kd*e' + Kp*e = 0
class ComputedTorqueController {
public:
    ComputedTorqueController(const RobotArm& arm, CtrlGains gains, TrajectoryParams traj)
        : arm_(arm), gains_(gains), traj_(traj) {}

    Eigen::Vector3d compute(const Eigen::Vector3d& q, const Eigen::Vector3d& dq,
                             double t) const {
        auto ref = desired_trajectory(t, traj_);
        const Eigen::Vector3d e   = ref.q  - q;
        const Eigen::Vector3d de  = ref.dq - dq;
        const Eigen::Vector3d v   = ref.ddq + gains_.Kd * de + gains_.Kp * e;
        auto d = arm_.dynamics(q, dq);
        return d.M * v + d.Cdq + d.gv;
    }

private:
    const RobotArm&   arm_;
    CtrlGains         gains_;
    TrajectoryParams  traj_;
};

} // namespace robot
