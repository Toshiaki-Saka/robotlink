/*
 * tlm_core.h — C ABI for the two-link manipulator workspace demo.
 *
 * Reproduces two_link_manipulator_workspace.py:
 *
 *   Forward kinematics for a planar 2-link arm with base at (0, 0):
 *     joint1 = (0, 0)
 *     joint2 = (l1 cos θ1,         l1 sin θ1)
 *     end    = (joint2.x + l2 cos(θ1+θ2),
 *               joint2.y + l2 sin(θ1+θ2))
 *
 *   Workspace: trace the end-effector along the four edges of the
 *   joint-angle rectangle  θ1 ∈ [θ1_min, θ1_max], θ2 ∈ [θ2_min, θ2_max].
 *   The Python script does this twice — once with θ2 ∈ [θ2_min, 0]
 *   (the "-θ2" boundary, drawn red) and once with θ2 ∈ [0, θ2_max]
 *   (the "+θ2" boundary, drawn green) — which together fill the
 *   reachable area.
 *
 * Frontends: Qt6 C++ / Avalonia C# / Python (ctypes). Windows-first.
 */
#ifndef TLM_CORE_H
#define TLM_CORE_H

#ifdef _WIN32
#  ifdef TLM_CORE_BUILD
#    define TLM_CORE_API __declspec(dllexport)
#  else
#    define TLM_CORE_API __declspec(dllimport)
#  endif
#else
#  define TLM_CORE_API __attribute__((visibility("default")))
#endif

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Robot configuration ------------------------------------------- */
typedef struct TlmRobotConfig {
    double l1;            /* link 1 length [m] */
    double l2;            /* link 2 length [m] */
    double theta1_min;    /* [rad] */
    double theta1_max;    /* [rad] */
    double theta2_min;    /* [rad] */
    double theta2_max;    /* [rad] */
    double base_x;        /* base position [m] */
    double base_y;        /* base position [m] */
} TlmRobotConfig;

TLM_CORE_API void tlm_core_default_config(TlmRobotConfig*);

/* ----- Forward kinematics ------------------------------------------- */
typedef struct TlmPose {
    double base_x,   base_y;
    double joint2_x, joint2_y;
    double end_x,    end_y;
} TlmPose;

/* Compute the pose of the arm for the given joint angles. Always
 * succeeds for finite inputs. Returns 0 on bad input. */
TLM_CORE_API int32_t tlm_core_forward_kinematics(const TlmRobotConfig*,
                                                 double theta1, double theta2,
                                                 TlmPose* out);

/* ----- Workspace boundary -------------------------------------------- */
/*
 * The boundary consists of 4 polylines per "side":
 *
 *   curve_a: θ2 = t2_lo, θ1 sweeps from t1_min to t1_max
 *   curve_b: θ2 = t2_hi, θ1 sweeps from t1_min to t1_max
 *   curve_c: θ1 = t1_min, θ2 sweeps from t2_lo to t2_hi
 *   curve_d: θ1 = t1_max, θ2 sweeps from t2_lo to t2_hi
 *
 * The Python script calls this once with (θ2_min, 0) — the "-θ2"
 * (red) boundary — and once with (0, θ2_max) — the "+θ2" (green)
 * boundary.
 *
 * Each side returns 4·samples_per_curve points. With samples_per_curve
 * = 100 (the Python default) that's 400 points per side.
 */
typedef struct TlmWorkspace TlmWorkspace;

TLM_CORE_API TlmWorkspace* tlm_core_compute_workspace(
    const TlmRobotConfig*,
    double t2_lo, double t2_hi,
    int32_t samples_per_curve);    /* >= 2 */

TLM_CORE_API void tlm_core_free_workspace(TlmWorkspace*);

/* Number of samples per curve (same value passed in). */
TLM_CORE_API int32_t tlm_core_ws_samples_per_curve(const TlmWorkspace*);

/* Each "copy" call fills buffer_len doubles for the named curve. The
 * buffer must hold samples_per_curve elements. Returns 0 on bad input,
 * else samples_per_curve. */
TLM_CORE_API int32_t tlm_core_ws_copy_a_x(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_a_y(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_b_x(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_b_y(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_c_x(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_c_y(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_d_x(const TlmWorkspace*, double*, int32_t);
TLM_CORE_API int32_t tlm_core_ws_copy_d_y(const TlmWorkspace*, double*, int32_t);

/* ----- Misc ---------------------------------------------------------- */
TLM_CORE_API const char* tlm_core_version(void);

#ifdef __cplusplus
}
#endif

#endif  /* TLM_CORE_H */
