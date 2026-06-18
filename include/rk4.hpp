#pragma once
#include <Eigen/Dense>

namespace robot {

// Generic RK4 step for Eigen-based state vectors.
// f: (state, t) -> d(state)/dt
template <typename State, typename DerivFn>
State rk4_step(const State& x, double t, double dt, DerivFn&& f) {
    const State k1 = f(x, t);
    const State k2 = f(x + 0.5 * dt * k1, t + 0.5 * dt);
    const State k3 = f(x + 0.5 * dt * k2, t + 0.5 * dt);
    const State k4 = f(x + dt * k3, t + dt);
    return x + (dt / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4);
}

} // namespace robot
