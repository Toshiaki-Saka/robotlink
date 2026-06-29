// smoke_test.cpp — verifies the core against expected FK values.

#include "tlm_core.h"

#include <cmath>
#include <cstdio>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;
}

int main() {
    std::printf("=== %s ===\n", tlm_core_version());

    TlmRobotConfig cfg;
    tlm_core_default_config(&cfg);
    std::printf("Config: l1=%.3f l2=%.3f  θ1∈[%+.4f, %+.4f]  θ2∈[%+.4f, %+.4f]\n",
                cfg.l1, cfg.l2, cfg.theta1_min, cfg.theta1_max,
                cfg.theta2_min, cfg.theta2_max);

    // Reference FK values computed from the Python formula.
    struct {
        const char* name;
        double t1, t2;
        double expected_end_x, expected_end_y;
    } cases[] = {
        {"θ1=0, θ2=0",        0.0,         0.0,        2.0,        0.0},
        {"θ1=π/4, θ2=π/4",    kPi/4.0,     kPi/4.0,    0.7071067,  1.7071067},
        {"θ1=π/2, θ2=0",      kPi/2.0,     0.0,        0.0,        2.0},
        {"θ1=0, θ2=π/2",      0.0,         kPi/2.0,    1.0,        1.0},
        {"θ1=-π/4, θ2=π/3",  -kPi/4.0,     kPi/3.0,    1.6730326, -0.4482877},
        {"θ1=π/2, θ2=π/2",    kPi/2.0,     kPi/2.0,   -1.0,        1.0},
    };

    int failures = 0;
    for (const auto& tc : cases) {
        TlmPose p;
        if (!tlm_core_forward_kinematics(&cfg, tc.t1, tc.t2, &p)) {
            std::fprintf(stderr, "FAIL: fk %s\n", tc.name);
            ++failures; continue;
        }
        const double dx = std::fabs(p.end_x - tc.expected_end_x);
        const double dy = std::fabs(p.end_y - tc.expected_end_y);
        std::printf("--- %-22s end=(%+10.7f, %+10.7f)  joint2=(%+8.4f, %+8.4f)  "
                    "(Δx=%.1e, Δy=%.1e)\n",
                    tc.name, p.end_x, p.end_y, p.joint2_x, p.joint2_y, dx, dy);
        if (dx > 1e-6 || dy > 1e-6) {
            std::printf("    FAIL: expected (%.7f, %.7f)\n",
                        tc.expected_end_x, tc.expected_end_y);
            ++failures;
        }
    }

    // Workspace boundary checks (-θ2 side, t2 ∈ [-π/2, 0]).
    std::printf("\nWorkspace boundary check:\n");
    TlmWorkspace* ws = tlm_core_compute_workspace(&cfg, cfg.theta2_min, 0.0, 100);
    if (!ws) { std::fprintf(stderr, "FAIL: workspace\n"); return 1; }

    if (tlm_core_ws_samples_per_curve(ws) != 100) {
        std::printf("    FAIL: samples_per_curve != 100\n");
        ++failures;
    }
    std::vector<double> ax(100), ay(100);
    tlm_core_ws_copy_a_x(ws, ax.data(), 100);
    tlm_core_ws_copy_a_y(ws, ay.data(), 100);
    // Curve A endpoint: θ1=+π/2, θ2=-π/2  ->  end = (1, 1).
    std::printf("    curve A sample 99: (%+8.4f, %+8.4f)  (expect (1, 1))\n",
                ax[99], ay[99]);
    if (std::fabs(ax[99] - 1.0) > 1e-6 || std::fabs(ay[99] - 1.0) > 1e-6) {
        std::printf("    FAIL: curve A endpoint != (1, 1)\n");
        ++failures;
    }
    tlm_core_free_workspace(ws);

    // Invalid input is rejected.
    TlmPose dummy;
    if (tlm_core_forward_kinematics(nullptr, 0.0, 0.0, &dummy)) {
        std::printf("    FAIL: NULL config accepted by forward_kinematics\n");
        ++failures;
    }

    if (failures == 0) std::printf("\nALL OK.\n");
    else                std::printf("\nFAILURES: %d\n", failures);
    return failures ? 1 : 0;
}
