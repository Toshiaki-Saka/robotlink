# Integration notes: why fixed-step RK4 is enough here

This document explains the choice of a fixed-step 4th-order Runge-Kutta (RK4)
integrator for RobotLink, and when you would want an adaptive solver instead.

## Current setup

| Item                         | Value                                  |
|------------------------------|----------------------------------------|
| Method                       | RK4 (fixed step, 4th order)            |
| Time step `dt`               | 0.005 s (`sim_config.hpp`)             |
| Total steps                  | 1,600 (8 s simulation)                 |
| Function evaluations / step  | 4 (`rk4.hpp`)                          |

## RK4 vs. adaptive solvers

**RK4 (used here)**
- Local truncation error $O(dt^5)$, global error $O(dt^4)$.
- Fixed step size: predictable cost, simple implementation.
- No error estimate and no accuracy guarantee.

**Adaptive Dormand-Prince (e.g. MATLAB `ode45`, Boost.Odeint `runge_kutta_dopri5`)**
- Computes a 4th- and 5th-order solution and uses the difference to estimate
  local error.
- Automatically shrinks or grows `dt` to meet a requested tolerance
  (e.g. `RelTol = 1e-3`, `AbsTol = 1e-6`).
- About 6 function evaluations per step (effectively 5 with the FSAL property).

## Why RK4 is sufficient for this arm

The closed-loop system uses computed-torque control with $K_p = 80$, $K_d = 18$,
which linearizes the error dynamics:

```
s^2 + 18 s + 80 = 0   →   s = -8, -10
```

RK4's stability region requires roughly $|\lambda \cdot dt| \le 2.79$. Here:

- $\lambda = -8$,  $dt = 0.005$  →  $\lambda \cdot dt = -0.04$  — comfortably stable
- $\lambda = -10$, $dt = 0.005$  →  $\lambda \cdot dt = -0.05$  — comfortably stable

The fastest reference-trajectory frequency is `ω = 1.2 rad/s`, giving on the
order of a thousand integration points per oscillation period. The system is
smooth and non-stiff, so the fixed step is more than accurate enough.

## When you would switch to an adaptive solver

| Situation                          | Fixed-step RK4 ($dt = 0.005$) | Adaptive solver           |
|-------------------------------------|-----------------------------|---------------------------|
| This arm (smooth, non-stiff)        | Accurate                    | Comparable, little gain   |
| High-gain / stiff system ($K_p \gg 80$)  | Needs a smaller `dt`        | Shrinks `dt` automatically|
| Discontinuous disturbance / contact | Accuracy degrades           | Step control handles it   |
| A guaranteed error bound is needed  | No guarantee                | Tolerance is specifiable  |

## The contact / collision case

A common point of confusion: bipedal-walking simulations often *require* an
adaptive solver, while this arm does not. The reason is **discontinuities**, not
the solver itself:

- **Walking**: foot-ground contact changes the leg velocity discontinuously
  (impact equations). The right-hand side $f(x, t)$ has discontinuity points. A
  fixed step integrates straight across them and accumulates large error.
  Adaptive solvers add **event detection** (zero-crossing) so the step can be
  stopped exactly at the impact and the state reset.

- **This arm**: there is no contact with the environment. The state $[q, \dot{q}]$
  and the right-hand side (dynamics + computed torque) are smooth in time. RK4's
  assumption of a sufficiently smooth solution holds.

**Rule of thumb**

> Smooth system, no discontinuous events → fixed-step RK4 is enough.
>
> Contact, collision, or switching → use an adaptive step with event detection.

If you raise the gains substantially (say $K_p > 500$) or add collision/contact
forces, consider moving to an adaptive Dormand-Prince integrator such as
Boost.Odeint's `runge_kutta_dopri5`.
