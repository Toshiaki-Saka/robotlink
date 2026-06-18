// Unit tests for the generic RK4 integrator, checked against problems with
// known closed-form solutions.

#include "test_harness.hpp"

#include <Eigen/Dense>
#include <cmath>

#include "rk4.hpp"

using Vec1 = Eigen::Matrix<double, 1, 1>;
using Vec2 = Eigen::Matrix<double, 2, 1>;

TEST("RK4 integrates exponential decay accurately") {
    // x' = -2x,  x(0) = 1  ->  x(t) = exp(-2t)
    auto f = [](const Vec1& x, double) -> Vec1 { return -2.0 * x; };

    Vec1 x;
    x << 1.0;
    const double dt = 0.01;
    const int steps = 200;  // integrate to t = 2
    for (int i = 0; i < steps; ++i)
        x = robot::rk4_step(x, i * dt, dt, f);

    const double exact = std::exp(-2.0 * steps * dt);
    CHECK_NEAR(x(0), exact, 1e-8);
}

TEST("RK4 integrates a harmonic oscillator accurately") {
    // x'' = -x  ->  state [x, v],  x(0)=1, v(0)=0  ->  x(t) = cos(t)
    auto f = [](const Vec2& s, double) -> Vec2 {
        Vec2 ds;
        ds(0) = s(1);
        ds(1) = -s(0);
        return ds;
    };

    Vec2 s;
    s << 1.0, 0.0;
    const double dt = 0.005;
    const int steps = 2000;  // integrate to t = 10
    for (int i = 0; i < steps; ++i)
        s = robot::rk4_step(s, i * dt, dt, f);

    const double t = steps * dt;
    CHECK_NEAR(s(0), std::cos(t), 1e-6);
    CHECK_NEAR(s(1), -std::sin(t), 1e-6);
}

TEST("RK4 conserves energy of a harmonic oscillator") {
    // For x'' = -x, the invariant E = x^2 + v^2 must stay (nearly) constant.
    auto f = [](const Vec2& s, double) -> Vec2 {
        Vec2 ds;
        ds(0) = s(1);
        ds(1) = -s(0);
        return ds;
    };

    Vec2 s;
    s << 1.0, 0.0;
    const double e0 = s(0) * s(0) + s(1) * s(1);
    const double dt = 0.005;
    for (int i = 0; i < 4000; ++i) {  // 20 s
        s = robot::rk4_step(s, i * dt, dt, f);
        const double e = s(0) * s(0) + s(1) * s(1);
        CHECK_NEAR(e, e0, 1e-4);
    }
}

TEST("RK4 reproduces a linear-in-time solution exactly") {
    // x' = 3,  x(0) = 0  ->  x(t) = 3t.  RK4 is exact for polynomials here.
    auto f = [](const Vec1&, double) -> Vec1 {
        Vec1 d;
        d << 3.0;
        return d;
    };

    Vec1 x;
    x << 0.0;
    const double dt = 0.1;
    for (int i = 0; i < 50; ++i)
        x = robot::rk4_step(x, i * dt, dt, f);

    CHECK_NEAR(x(0), 3.0 * 50 * dt, 1e-10);
}

int main() {
    return robotlink_test::run_all();
}
