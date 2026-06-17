// tlm_core.cpp — two-link manipulator forward kinematics + workspace.

#include "tlm_core.h"

#include <cmath>
#include <new>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

bool config_ok(const TlmRobotConfig& c) {
    return std::isfinite(c.l1) && std::isfinite(c.l2)
        && c.l1 > 0.0 && c.l2 > 0.0
        && std::isfinite(c.theta1_min) && std::isfinite(c.theta1_max)
        && std::isfinite(c.theta2_min) && std::isfinite(c.theta2_max)
        && c.theta1_max >= c.theta1_min
        && c.theta2_max >= c.theta2_min
        && std::isfinite(c.base_x) && std::isfinite(c.base_y);
}

// Forward kinematics returning the three landmark points. Pure function —
// no allocation, no globals, identical formula to the Python reference.
void fk(const TlmRobotConfig& c, double t1, double t2, TlmPose& p) {
    p.base_x = c.base_x;
    p.base_y = c.base_y;
    p.joint2_x = p.base_x + c.l1 * std::cos(t1);
    p.joint2_y = p.base_y + c.l1 * std::sin(t1);
    p.end_x    = p.joint2_x + c.l2 * std::cos(t1 + t2);
    p.end_y    = p.joint2_y + c.l2 * std::sin(t1 + t2);
}

// One-shot linspace evaluation of fk along an edge of the joint-angle
// rectangle. `sweep_theta1` controls which joint moves: if true the
// sweep parameter is θ1 (and `fixed` is θ2), otherwise vice versa.
void sweep_edge(const TlmRobotConfig& c,
                double lo, double hi, double fixed,
                bool sweep_theta1, int samples,
                std::vector<double>& xs, std::vector<double>& ys)
{
    xs.resize(samples);
    ys.resize(samples);
    if (samples == 1) {
        TlmPose p;
        const double a = sweep_theta1 ? lo : fixed;
        const double b = sweep_theta1 ? fixed : lo;
        fk(c, a, b, p);
        xs[0] = p.end_x; ys[0] = p.end_y;
        return;
    }
    const double step = (hi - lo) / (samples - 1);
    for (int i = 0; i < samples; ++i) {
        const double t = lo + step * i;
        TlmPose p;
        const double a = sweep_theta1 ? t     : fixed;
        const double b = sweep_theta1 ? fixed : t;
        fk(c, a, b, p);
        xs[i] = p.end_x;
        ys[i] = p.end_y;
    }
}

}  // namespace

struct TlmWorkspace {
    int samples;
    std::vector<double> ax, ay;
    std::vector<double> bx, by;
    std::vector<double> cx, cy;
    std::vector<double> dx, dy;
};

extern "C" {

TLM_CORE_API const char* tlm_core_version(void) {
    return "tlm_core 1.0.0";
}

TLM_CORE_API void tlm_core_default_config(TlmRobotConfig* c) {
    if (!c) return;
    c->l1 = 1.0;
    c->l2 = 1.0;
    c->theta1_min = -kPi / 2.0;
    c->theta1_max =  kPi / 2.0;
    c->theta2_min = -kPi / 2.0;
    c->theta2_max =  kPi / 2.0;
    c->base_x = 0.0;
    c->base_y = 0.0;
}

TLM_CORE_API int32_t tlm_core_forward_kinematics(const TlmRobotConfig* c,
                                                 double t1, double t2,
                                                 TlmPose* out)
{
    if (!c || !out || !config_ok(*c)) return 0;
    if (!std::isfinite(t1) || !std::isfinite(t2)) return 0;
    fk(*c, t1, t2, *out);
    return 1;
}

TLM_CORE_API TlmWorkspace* tlm_core_compute_workspace(
    const TlmRobotConfig* c,
    double t2_lo, double t2_hi,
    int32_t samples_per_curve)
{
    if (!c || !config_ok(*c)) return nullptr;
    if (!std::isfinite(t2_lo) || !std::isfinite(t2_hi)) return nullptr;
    if (samples_per_curve < 2) return nullptr;

    auto* ws = new (std::nothrow) TlmWorkspace;
    if (!ws) return nullptr;
    ws->samples = samples_per_curve;

    // Same four edges the Python script traces. Note that the Python
    // reference uses np.linspace(t1min, t1max, 100) — an inclusive linear
    // sweep — which is exactly what sweep_edge above implements.
    sweep_edge(*c, c->theta1_min, c->theta1_max, t2_lo, /*sweep_theta1=*/true,
               samples_per_curve, ws->ax, ws->ay);
    sweep_edge(*c, c->theta1_min, c->theta1_max, t2_hi, /*sweep_theta1=*/true,
               samples_per_curve, ws->bx, ws->by);
    sweep_edge(*c, t2_lo,         t2_hi,         c->theta1_min, /*sweep_theta1=*/false,
               samples_per_curve, ws->cx, ws->cy);
    sweep_edge(*c, t2_lo,         t2_hi,         c->theta1_max, /*sweep_theta1=*/false,
               samples_per_curve, ws->dx, ws->dy);
    return ws;
}

TLM_CORE_API void tlm_core_free_workspace(TlmWorkspace* ws) { delete ws; }

TLM_CORE_API int32_t tlm_core_ws_samples_per_curve(const TlmWorkspace* ws) {
    return ws ? ws->samples : 0;
}

static int32_t copy_vec(const std::vector<double>& v, double* buf, int32_t cap) {
    if (!buf) return 0;
    const int32_t n = static_cast<int32_t>(v.size());
    if (cap < n) return 0;
    for (int32_t i = 0; i < n; ++i) buf[i] = v[i];
    return n;
}

TLM_CORE_API int32_t tlm_core_ws_copy_a_x(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->ax, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_a_y(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->ay, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_b_x(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->bx, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_b_y(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->by, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_c_x(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->cx, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_c_y(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->cy, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_d_x(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->dx, b, c) : 0; }
TLM_CORE_API int32_t tlm_core_ws_copy_d_y(const TlmWorkspace* w, double* b, int32_t c) { return w ? copy_vec(w->dy, b, c) : 0; }

}  // extern "C"
