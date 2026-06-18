# Dynamics and Equations of Motion

This document derives the full equations of motion for the 3-DOF robot arm in RobotLink —
from generalized coordinate definition, through the Lagrangian formulation and Euler-Lagrange
equations, to the computed-torque controller and numerical integration.

All expressions correspond directly to `python/derive_and_export.py` (symbolic derivation),
`include/sim_config.hpp` (parameters), and `include/controller.hpp` (control law).

---

## 1. Generalized Coordinates

The arm has 3 revolute joints. The generalized coordinate vector is

$$\mathbf{q} = \begin{pmatrix} q_1 \\ q_2 \\ q_3 \end{pmatrix} \in \mathbb{R}^3, \qquad \dot{\mathbf{q}} = \frac{d\mathbf{q}}{dt}, \qquad \ddot{\mathbf{q}} = \frac{d^2\mathbf{q}}{dt^2}$$

| Symbol | Joint type | Axis | Physical meaning |
|--------|-----------|------|-----------------|
| $q_1$ | Revolute | World $z$ | Yaw — rotates the entire arm in the horizontal plane |
| $q_2$ | Revolute | $R_z(q_1)\,\hat{y}$ | Shoulder pitch — raises/lowers link 2 in the vertical plane |
| $q_3$ | Revolute | $R_z(q_1)\,\hat{y}$ | Elbow pitch — bends link 3 relative to link 2 |

Units are radians. Joints 2 and 3 share the same pitch axis (after applying the yaw $q_1$),
so this is a **yaw–pitch–pitch** configuration.

---

## 2. Rotation Matrices

The two elementary rotation matrices used are:

$$R_z(\theta) = \begin{pmatrix} \cos\theta & -\sin\theta & 0 \\ \sin\theta & \cos\theta & 0 \\ 0 & 0 & 1 \end{pmatrix}, \qquad R_y(\theta) = \begin{pmatrix} \cos\theta & 0 & \sin\theta \\ 0 & 1 & 0 \\ -\sin\theta & 0 & \cos\theta \end{pmatrix}$$

The orientation of each link in the world frame is:

$$R_{12} = R_z(q_1)\,R_y(q_2), \qquad R_{123} = R_z(q_1)\,R_y(q_2+q_3)$$

Because joints 2 and 3 both rotate about the same axis $R_z(q_1)\hat{y}$, their pitch
angles simply add in $R_{123}$.

---

## 3. Kinematics

### 3.1 Center-of-Mass Positions

Each link is modeled as a **uniform rod**; its center of mass lies at the midpoint along
its local $x$-axis. Let $\hat{x} = (1,0,0)^\top$.

**Link 2** (length $L_2$, mass $m_2$):

$$\mathbf{p}_{c2} = R_{12}\,\frac{L_2}{2}\,\hat{x} = \frac{L_2}{2} \begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix}$$

**Link 3** (length $L_3$, mass $m_3$): the origin of link 3 is at the distal end of link 2,

$$\mathbf{p}_{c3} = R_{12}\,L_2\,\hat{x} + R_{123}\,\frac{L_3}{2}\,\hat{x} = L_2\begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix} + \frac{L_3}{2}\begin{pmatrix} \cos q_1\cos(q_2+q_3) \\ \sin q_1\cos(q_2+q_3) \\ -\sin(q_2+q_3) \end{pmatrix}$$

### 3.2 Forward Kinematics (End-Effector)

The joint positions (implemented in `robot_arm.hpp`):

$$\mathbf{p}_\text{elbow} = L_2 \begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix}$$

$$\mathbf{p}_\text{hand} = \mathbf{p}_\text{elbow} + L_3 \begin{pmatrix} \cos q_1\cos(q_2+q_3) \\ \sin q_1\cos(q_2+q_3) \\ -\sin(q_2+q_3) \end{pmatrix}$$

### 3.3 Center-of-Mass Velocities

Differentiating with respect to time:

$$\dot{\mathbf{p}}_{c2} = \frac{d}{dt}\left[R_{12}\,\frac{L_2}{2}\,\hat{x}\right], \qquad \dot{\mathbf{p}}_{c3} = \frac{d}{dt}\left[R_{12}\,L_2\,\hat{x} + R_{123}\,\frac{L_3}{2}\,\hat{x}\right]$$

These are computed symbolically by SymPy (`sp.diff(p_c2, t)`, `sp.diff(p_c3, t)`).

### 3.4 Angular Velocities

Define the two joint axes in the world frame:

$$\hat{z} = \begin{pmatrix}0\\0\\1\end{pmatrix}, \qquad \hat{y}_\text{rot} = R_z(q_1)\begin{pmatrix}0\\1\\0\end{pmatrix} = \begin{pmatrix}-\sin q_1\\\cos q_1\\0\end{pmatrix}$$

Note that $\hat{z} \perp \hat{y}_\text{rot}$ (orthonormal pair).

Angular velocity of each link is assembled by summing contributions from all joints up to
and including that link:

$$\boldsymbol{\omega}_2 = \dot{q}_1\,\hat{z} + \dot{q}_2\,\hat{y}_\text{rot}$$

$$\boldsymbol{\omega}_3 = \dot{q}_1\,\hat{z} + (\dot{q}_2+\dot{q}_3)\,\hat{y}_\text{rot}$$

The **longitudinal axes** of the links (along each rod's length in the world frame) are:

$$\hat{a}_2 = R_{12}\,\hat{x} = \begin{pmatrix}\cos q_1\cos q_2\\\sin q_1\cos q_2\\-\sin q_2\end{pmatrix}, \qquad \hat{a}_3 = R_{123}\,\hat{x} = \begin{pmatrix}\cos q_1\cos(q_2+q_3)\\\sin q_1\cos(q_2+q_3)\\-\sin(q_2+q_3)\end{pmatrix}$$

---

## 4. Lagrangian

The Lagrangian is $\mathcal{L} = T - U$.

### 4.1 Kinetic Energy

**Translational kinetic energy** of each link's center of mass:

$$T_{\text{trans},k} = \frac{1}{2}\,m_k\,\dot{\mathbf{p}}_{ck}^\top\dot{\mathbf{p}}_{ck}, \qquad k = 2,3$$

**Rotational kinetic energy**: for a uniform rod of mass $m$ and length $L$, the inertia
tensor about the center of mass has transverse moment $I_\perp = mL^2/12$ and zero axial
moment (spinning about the rod's own axis is neglected). The rotational kinetic energy is
therefore:

$$T_{\text{rot}} = \frac{1}{2}\,I_\perp\,\|\boldsymbol{\omega}_\perp\|^2 = \frac{1}{2}\cdot\frac{mL^2}{12}\cdot\left(\|\boldsymbol{\omega}\|^2 - (\boldsymbol{\omega}\cdot\hat{a})^2\right) = \frac{mL^2}{24}\left(\|\boldsymbol{\omega}\|^2 - (\boldsymbol{\omega}\cdot\hat{a})^2\right)$$

where $\boldsymbol{\omega}_\perp = \boldsymbol{\omega} - (\boldsymbol{\omega}\cdot\hat{a})\hat{a}$ is the component of $\boldsymbol{\omega}$ perpendicular to the rod.

**Total kinetic energy**:

$$T = \frac{1}{2}m_2\,\dot{\mathbf{p}}_{c2}^\top\dot{\mathbf{p}}_{c2} + \frac{m_2 L_2^2}{24}\!\left(\|\boldsymbol{\omega}_2\|^2 - (\boldsymbol{\omega}_2\cdot\hat{a}_2)^2\right) + \frac{1}{2}m_3\,\dot{\mathbf{p}}_{c3}^\top\dot{\mathbf{p}}_{c3} + \frac{m_3 L_3^2}{24}\!\left(\|\boldsymbol{\omega}_3\|^2 - (\boldsymbol{\omega}_3\cdot\hat{a}_3)^2\right)$$

Because $T$ is quadratic in $\dot{\mathbf{q}}$, it can always be written as:

$$T = \frac{1}{2}\,\dot{\mathbf{q}}^\top M(\mathbf{q})\,\dot{\mathbf{q}}$$

for some configuration-dependent symmetric positive-definite matrix $M(\mathbf{q})$.

### 4.2 Potential Energy

With the world $z$-axis pointing upward and the arm base at $z = 0$:

$$U = m_2\,g\,(\mathbf{p}_{c2})_z + m_3\,g\,(\mathbf{p}_{c3})_z$$

Substituting the $z$-components from Section 3.1:

$$U = -\frac{m_2\,g\,L_2}{2}\sin q_2 + m_3\,g\!\left(-L_2\sin q_2 - \frac{L_3}{2}\sin(q_2+q_3)\right)$$

$$\boxed{U = -\left(\frac{m_2}{2}+m_3\right)g L_2\sin q_2 - \frac{m_3\,g\,L_3}{2}\sin(q_2+q_3)}$$

Note that $U$ is independent of $q_1$ — yaw rotation does not change link heights.

---

## 5. Euler-Lagrange Equations

The Euler-Lagrange equation for generalized coordinate $q_i$ is:

$$\frac{d}{dt}\!\left(\frac{\partial\mathcal{L}}{\partial\dot{q}_i}\right) - \frac{\partial\mathcal{L}}{\partial q_i} = \tau_i, \qquad i = 1,2,3$$

Expanding with $\mathcal{L} = T - U$ and using $\partial U/\partial\dot{q}_i = 0$:

$$\frac{d}{dt}\!\left(\frac{\partial T}{\partial\dot{q}_i}\right) - \frac{\partial T}{\partial q_i} + \frac{\partial U}{\partial q_i} = \tau_i$$

In the SymPy derivation (`derive_and_export.py`), this is computed as:

```python
raw = sp.diff(sp.diff(Lag, dqi), t) - sp.diff(Lag, qi)
EOM[i] = sp.trigsimp(raw)
```

---

## 6. Equations of Motion

Expanding the kinematic expressions and collecting terms by $\ddot{\mathbf{q}}$,
$\dot{\mathbf{q}}\otimes\dot{\mathbf{q}}$, and $\mathbf{q}$-only parts yields the
standard robot dynamics form:

$$\boxed{M(\mathbf{q})\,\ddot{\mathbf{q}} + C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} + \mathbf{g}(\mathbf{q}) = \boldsymbol{\tau}}$$

### 6.1 Mass Matrix $M(\mathbf{q})$

$M(\mathbf{q}) \in \mathbb{R}^{3\times3}$ is extracted as the coefficient of $\ddot{\mathbf{q}}$:

$$M_{ij}(\mathbf{q}) = \frac{\partial\,[\text{EOM}_i]}{\partial\ddot{q}_j}$$

In code:
```python
M_mat[i, j] = sp.trigsimp(sp.diff(EOM_i[i], DDQ[j]))
```

**Properties** (verified by `tests/test_dynamics.cpp`):

- **Symmetry**: $M = M^\top$, i.e.\ $M_{ij} = M_{ji}$
- **Positive definiteness**: $\mathbf{v}^\top M\,\mathbf{v} > 0$ for all $\mathbf{v}\neq\mathbf{0}$
- **Configuration dependence**: only $q_2$ and $q_3$ appear; $q_1$ does not enter $M$
  (the arm's inertia about the yaw axis is the same at every yaw angle)

The diagonal entries represent the effective inertia for each joint; off-diagonal entries
represent dynamic coupling between joints.

### 6.2 Coriolis and Centrifugal Term $C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}}$

The Coriolis/centrifugal vector $\mathbf{c} = C\dot{\mathbf{q}} \in \mathbb{R}^3$ is
the $\ddot{\mathbf{q}}$-free, $\dot{\mathbf{q}}$-dependent part of the Euler-Lagrange
equations:

$$c_i = \left.[\text{EOM}_i]\right|_{\ddot{\mathbf{q}}=\mathbf{0}} - g_i = \sum_{j,k}\Gamma_{ijk}\,\dot{q}_j\dot{q}_k$$

where $\Gamma_{ijk}$ are the Christoffel symbols of the first kind:

$$\Gamma_{ijk} = \frac{1}{2}\!\left(\frac{\partial M_{ij}}{\partial q_k} + \frac{\partial M_{ik}}{\partial q_j} - \frac{\partial M_{jk}}{\partial q_i}\right)$$

In code:
```python
rest  = EOM evaluated with ddq = 0
g_vec = rest evaluated with dq  = 0
Cqdq  = rest - g_vec
```

**Energy consistency**: the matrix $C$ can be chosen (via the Christoffel-symbol
definition above) so that $\dot{M} - 2C$ is skew-symmetric. This guarantees:

$$\dot{\mathbf{q}}^\top C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} = 0$$

meaning Coriolis/centrifugal forces do no net work on the system — a consequence of
Newton's third law for the internal coupling forces.

### 6.3 Gravity Vector $\mathbf{g}(\mathbf{q})$

$\mathbf{g} \in \mathbb{R}^3$ is the gradient of the potential energy:

$$g_i(\mathbf{q}) = \frac{\partial U}{\partial q_i}$$

Differentiating the expression from Section 4.2:

$$g_1 = 0$$

$$g_2 = -\left(\frac{m_2}{2}+m_3\right)g L_2\cos q_2 - \frac{m_3\,g\,L_3}{2}\cos(q_2+q_3)$$

$$g_3 = -\frac{m_3\,g\,L_3}{2}\cos(q_2+q_3)$$

$g_1 = 0$ because rotating about the vertical axis ($q_1$ is a pure yaw) does not
change the height of any link.

**Static equilibrium check**: setting $\boldsymbol{\tau} = \mathbf{g}(\mathbf{q})$,
$\dot{\mathbf{q}} = \ddot{\mathbf{q}} = \mathbf{0}$ satisfies the equations of motion
identically. This is tested in `tests/test_dynamics.cpp`.

---

## 7. Forward Dynamics

Given applied torques $\boldsymbol{\tau}$, solve for joint accelerations:

$$\ddot{\mathbf{q}} = M(\mathbf{q})^{-1}\!\left[\boldsymbol{\tau} - C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} - \mathbf{g}(\mathbf{q})\right]$$

In `robot_arm.hpp`, $M$ is positive definite, so Cholesky decomposition is used:

```cpp
d.M.llt().solve(tau - d.Cdq - d.gv)
```

The full 6-dimensional state vector for integration is:

$$\mathbf{x} = \begin{pmatrix}\mathbf{q}\\\dot{\mathbf{q}}\end{pmatrix} \in \mathbb{R}^6, \qquad \dot{\mathbf{x}} = f(\mathbf{x},t) = \begin{pmatrix}\dot{\mathbf{q}}\\M(\mathbf{q})^{-1}\!\left[\boldsymbol{\tau}(t,\mathbf{q},\dot{\mathbf{q}}) - C\dot{\mathbf{q}} - \mathbf{g}\right]\end{pmatrix}$$

---

## 8. Computed-Torque Control

### 8.1 Control Law

The computed-torque (inverse-dynamics) controller cancels the nonlinear arm dynamics
and imposes desired linear error dynamics. Let:

$$\mathbf{e}(t) = \mathbf{q}_d(t) - \mathbf{q}(t)$$

The control law is:

$$\boxed{\boldsymbol{\tau} = M(\mathbf{q})\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right] + C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} + \mathbf{g}(\mathbf{q})}$$

where $K_p = k_p I_3$ and $K_d = k_d I_3$ are scalar gain matrices.

### 8.2 Closed-Loop Error Dynamics

Substitute the control law into $M\ddot{\mathbf{q}} + C\dot{\mathbf{q}} + \mathbf{g} = \boldsymbol{\tau}$:

$$M\ddot{\mathbf{q}} + C\dot{\mathbf{q}} + \mathbf{g} = M\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right] + C\dot{\mathbf{q}} + \mathbf{g}$$

The $C\dot{\mathbf{q}}$ and $\mathbf{g}$ terms cancel exactly (this cancellation relies on
accurate knowledge of the dynamics — the computed-torque controller is a form of
**exact linearization**):

$$M\ddot{\mathbf{q}} = M\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right]$$

Since $M$ is invertible:

$$\ddot{\mathbf{q}} = \ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}$$

Using $\ddot{\mathbf{e}} = \ddot{\mathbf{q}}_d - \ddot{\mathbf{q}}$:

$$\boxed{\ddot{\mathbf{e}} + K_d\,\dot{\mathbf{e}} + K_p\,\mathbf{e} = \mathbf{0}}$$

The nonlinear arm dynamics have been completely replaced by a **linear, decoupled,
time-invariant** system.

### 8.3 Pole Placement

Because $K_p = k_p I$ and $K_d = k_d I$, the error dynamics decouple into three
identical scalar systems. The characteristic polynomial for each joint is:

$$s^2 + k_d\,s + k_p = 0$$

With the default gains $k_p = 80$, $k_d = 18$:

$$s^2 + 18s + 80 = (s+8)(s+10) = 0 \implies s_{1,2} = -8,\;-10$$

**Overdamped response** (no oscillation in the tracking error):

$$\Delta = k_d^2 - 4k_p = 324 - 320 = 4 > 0$$

The closed-loop time constants are $\tau_1 = 1/8 = 0.125\,\text{s}$ and $\tau_2 = 1/10 = 0.1\,\text{s}$.

For a step disturbance, the error decays approximately as:

$$\|\mathbf{e}(t)\| \sim C_1\,e^{-8t} + C_2\,e^{-10t}$$

### 8.4 RK4 Stability with the Closed-Loop Poles

The fixed-step RK4 integrator has a stability region that requires $|\lambda\,\Delta t| \lesssim 2.79$
for real negative eigenvalues. With $\Delta t = 0.005\,\text{s}$:

| Pole | $|\lambda\,\Delta t|$ | RK4 stable? |
|------|----------------------|-------------|
| $s = -8$ | $0.040$ | Yes — margin $70\times$ |
| $s = -10$ | $0.050$ | Yes — margin $56\times$ |

See [`docs/integration-notes.md`](integration-notes.md) for the full RK4 discussion.

---

## 9. Reference Trajectory

The desired trajectory for joint $i$ is a sinusoid with offset:

$$q_{d,i}(t) = \delta_i + A_i\sin(\omega_i\,t + \varphi_i)$$

$$\dot{q}_{d,i}(t) = A_i\,\omega_i\cos(\omega_i\,t + \varphi_i)$$

$$\ddot{q}_{d,i}(t) = -A_i\,\omega_i^2\sin(\omega_i\,t + \varphi_i)$$

Default parameter values (from `include/sim_config.hpp`):

| Parameter | Joint 1 | Joint 2 | Joint 3 |
|-----------|--------:|--------:|--------:|
| Amplitude $A_i$ [rad] | 0.6 | 0.5 | 0.7 |
| Frequency $\omega_i$ [rad/s] | 0.8 | 1.0 | 1.2 |
| Phase $\varphi_i$ [rad] | $0$ | $\pi/4$ | $\pi/2$ |
| Offset $\delta_i$ [rad] | $0$ | $-0.3$ | $0.8$ |

---

## 10. RK4 Integration Summary

The ODE $\dot{\mathbf{x}} = f(\mathbf{x}, t)$ is advanced with a fixed-step 4th-order
Runge-Kutta scheme (`include/rk4.hpp`), using four evaluations per step:

$$k_1 = f(\mathbf{x}_n,\,t_n)$$

$$k_2 = f\!\left(\mathbf{x}_n + \tfrac{\Delta t}{2}k_1,\; t_n + \tfrac{\Delta t}{2}\right)$$

$$k_3 = f\!\left(\mathbf{x}_n + \tfrac{\Delta t}{2}k_2,\; t_n + \tfrac{\Delta t}{2}\right)$$

$$k_4 = f\!\left(\mathbf{x}_n + \Delta t\,k_3,\; t_n + \Delta t\right)$$

$$\mathbf{x}_{n+1} = \mathbf{x}_n + \frac{\Delta t}{6}\!\left(k_1 + 2k_2 + 2k_3 + k_4\right)$$

Global truncation error is $O(\Delta t^4)$. With $\Delta t = 0.005\,\text{s}$ over
$T = 8\,\text{s}$, the integrator takes 1,600 steps and calls $f$ exactly 6,400 times.

---

## 11. Parameter Summary

Physical parameters are defined in `include/sim_config.hpp`:

| Symbol | Value | Description |
|--------|------:|-------------|
| $L_2$ | 0.5 m | Length of link 2 |
| $L_3$ | 0.4 m | Length of link 3 |
| $m_2$ | 1.0 kg | Mass of link 2 |
| $m_3$ | 0.7 kg | Mass of link 3 |
| $g$ | 9.81 m/s² | Gravitational acceleration |
| $k_p$ | 80 | Proportional gain (per joint) |
| $k_d$ | 18 | Derivative gain (per joint) |
| $\Delta t$ | 0.005 s | RK4 step size |
| $T_\text{end}$ | 8.0 s | Simulation duration |
