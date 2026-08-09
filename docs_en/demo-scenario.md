# The demo scenario: what the arm is trying to do

The animation and the plots shipped with this repository come from a single
8-second simulation run (`src/main.cpp`). This document explains what that run is
supposed to show, why the arm moves the way it does, and what a "good" result
looks like.

## The one-sentence version

The arm starts in a **deliberately wrong pose** and is commanded to follow a
**smooth, three-joint, multi-frequency reference motion**; the demo asks whether
the symbolically derived dynamics plus the computed-torque controller can (a)
remove that initial error quickly and without overshoot, and (b) then keep
tracking the moving target essentially exactly.

It is a *tracking* demo, not a pick-and-place or path-planning demo. Nothing is
grasped, and there is no obstacle or task goal — the "task" is the reference
trajectory itself.

## The arm

Three revolute joints, two moving links (`include/sim_config.hpp`):

| Joint | Name           | Axis                                   |
|-------|----------------|----------------------------------------|
| q1    | shoulder yaw   | rotation about the vertical base axis  |
| q2    | shoulder pitch | lifts the upper link out of horizontal |
| q3    | elbow          | flexes the forearm relative to the upper link |

Upper link `L2 = 0.5 m`, `m2 = 1.0 kg`; forearm `L3 = 0.4 m`, `m3 = 0.7 kg`;
gravity acts along −z. Maximum reach is `L2 + L3 = 0.9 m`.

## The commanded motion

Every joint follows an independent sine wave:

```
q_d,i(t) = offset_i + A_i * sin(w_i * t + phi_i)
```

| Joint | offset (rad) | A (rad) | w (rad/s) | phi   | Resulting range      | Period  |
|-------|--------------|---------|-----------|-------|----------------------|---------|
| q1    | 0.0          | 0.6     | 0.8       | 0     | −0.60 … +0.60 rad (±34.4°) | 7.85 s |
| q2    | −0.3         | 0.5     | 1.0       | π/4   | −0.80 … +0.20 rad (−45.8°…+11.5°) | 6.28 s |
| q3    | +0.8         | 0.7     | 1.2       | π/2   | +0.10 … +1.50 rad (5.7°…86.0°) | 5.24 s |

In plain terms: the base **sweeps left and right** through roughly ±34°, the
shoulder **rocks up and down** around a slightly lowered pose, and the elbow
**flexes and extends** between nearly straight and a right angle — all at the
same time, but each at its own rate.

Because the three rates are in the ratio 4 : 5 : 6, the joints never repeat the
same combination within the run: the pattern would only close after 31.4 s, so
the 8-second demo shows about a quarter of it. The hand therefore traces a
non-repeating three-dimensional Lissajous-like curve, staying in a shell between
0.48 m and 0.90 m from the base — this is the swirling path drawn in the 3D
panel of the visualizer.

## Why *this* motion and not a step or a straight line

The trajectory was chosen so that the demo actually exercises the parts of the
pipeline that are hard to get right.

**1. It is smooth, so the controller has a legal feedforward term.** Computed
torque needs the desired acceleration $\ddot{q}_d$ as an input. A sine is
infinitely differentiable, so `desired_trajectory()` returns $q_d$, $\dot{q}_d$
and $\ddot{q}_d$ in closed form and all three are continuous. A step command
would demand an impulsive torque and would tell you nothing about tracking
quality.

**2. All three joints move at once, at different frequencies.** This is the main
point. When only one joint moves, or when the motion is slow enough to be
quasi-static, the Coriolis/centrifugal term $C(q,\dot{q})\dot{q}$ and the
configuration dependence of the mass matrix $M(q)$ are nearly invisible — and
those are exactly the terms that are laborious and error-prone to derive by hand,
i.e. the reason this project derives them symbolically. Detuned frequencies keep
the arm out of any synchronized, symmetric configuration, so the coupling terms
stay excited for the whole run. A sign error in the generated dynamics shows up
immediately as a tracking error.

**3. The gravity load varies over a wide range.** With q2 sweeping from −45.8° to
+11.5° and q2 + q3 from −24.5° to +91.6°, the arm passes through nearly fully
extended horizontal poses (large gravity torque) and folded, near-vertical poses
(small gravity torque). This checks the gravity vector $g(q)$ across the working
range rather than at one operating point. The joint-2 torque, which carries most
of the load, swings over roughly −9.3 … +10.6 N·m.

**4. It stays inside the workspace and away from singularities.** The elbow angle
stays in 0.10 … 1.50 rad and never reaches 0, so the arm never straightens
completely (`q3 = 0` is the boundary singularity); the hand's horizontal distance
from the base axis never drops below 0.48 m, so it never approaches the shoulder
axis either. That is deliberate: this demo is about tracking accuracy, so
singularity behaviour is kept out of the picture. (Peak reach is 0.898 m of the
0.900 m available, so the arm does come close to full extension — a good place to
watch if you increase the amplitudes.)

## The deliberate initial error

The simulation starts from

```
q(0)  = [ 0.2, -0.1,  0.3] rad     (at rest, dq(0) = 0)
q_d(0) = [ 0.0,  0.054, 1.5] rad
```

so the arm begins **0.2 / 0.15 / 1.2 rad away from where it is told to be** —
about 69° of error on the elbow alone, which is 0.57 m of hand-position error.
This is not sloppiness; it is the second half of the test. Computed-torque
control claims to reduce the closed-loop error to the linear system

```
e'' + Kd * e' + Kp * e = 0        Kp = 80, Kd = 18  →  poles at s = -8, -10
```

and the only way to see that is to inject an error and watch it decay. With
those poles the response should be non-oscillatory with a time constant of
1/8 s — and in the run it is: the joint error falls below 0.01 rad by
t ≈ 0.78 s and below 0.001 rad by t ≈ 1.08 s, with no overshoot (the elbow error
decays from +1.2 rad to zero without ever changing sign).

This is also why the visualizer only measures steady-state accuracy after
**t = 2 s** (the dotted vertical line in the error panel): before that, the plot
is dominated by the intentional start-up transient.

## What a good result looks like

| Panel in the visualizer | What to check |
|-------------------------|---------------|
| q1 / q2 / q3 vs. time   | after ~0.5 s the solid "actual" curve sits on top of the dashed "desired" curve |
| joint error [deg]       | one decaying spike at the start, then a flat line at zero; no ringing |
| torque [N·m]            | smooth curves, no chattering; nonzero even when the arm is momentarily still, because gravity must be held |
| hand position error [mm]| large at t = 0 (~568 mm), then ~0; the title reports the steady-state RMS |
| 3D path                 | the actual hand path lies on the desired swirling path |

In the reference run the steady-state hand RMS error is about `5e-5 mm`, i.e.
tracking is exact down to integration round-off. That is the expected outcome:
the controller uses the *same* model the plant uses, so with no disturbance and
no model mismatch there is nothing left to track badly. If you see a
steady-state error here, something in the generated dynamics or the controller is
wrong — which is precisely what makes this demo a useful regression check.

## Making it your own experiment

Everything above lives in `TrajectoryParams` in `include/sim_config.hpp` (and the
initial state in `src/main.cpp`). Edit and rebuild:

| Change | What it demonstrates |
|--------|----------------------|
| `A = (0, 0, 0.7)` | isolates the elbow — the other joints must hold still against the reaction torques the moving elbow generates |
| double all `w` | higher speeds → larger Coriolis/centrifugal terms and larger torques; a good stress test for the fixed step size (see [`integration-notes.md`](integration-notes.md)) |
| `A = (0, 0, 0)` | turns the demo into a set-point regulation / gravity-hold test |
| a larger initial state offset in `src/main.cpp` | lengthens the transient and shows the error dynamics more clearly |
| lower `Kp`, `Kd` in `CtrlGains` | slower, possibly oscillatory convergence — the poles move accordingly |

## See also

- [`dynamics.md`](dynamics.md) — how the equations of motion are derived
- [`integration-notes.md`](integration-notes.md) — why fixed-step RK4 is enough
  for this trajectory
