# Two-Link Manipulator — one C++ core, three language frontends

A minimal example of sharing **one dependency-free C++ numerical core over a
stable C ABI** with three GUI frontends — Qt6 (C++), Avalonia (C#), and
PySide6 / matplotlib (Python). The payload is a planar two-link manipulator
(forward kinematics + workspace boundary); the *point* is the cross-language
architecture, not the robotics.

> **What this is.** A C-ABI multi-frontend **template**, not a robotics
> library — the planar 2-link FK is textbook trigonometry chosen because it is
> small and easy to verify. For the full treatment (3-DOF dynamics, SymPy →
> C++ codegen, RK4 + computed-torque control) see the sibling **robotlink
> (arm3dof)**; this `tlm` is its planar, FK-only little brother. The reusable
> assets here are the clean C ABI and the Avalonia 11 / Qt6×vcpkg
> troubleshooting notes under [`docs/`](docs/).

```
┌──────────────────────────────────────────────────────────────────────┐
│            C++ core  (tlm_core.dll / .so / .dylib)                   │
│            ─ Forward kinematics (planar 2-link arm)                  │
│            ─ Workspace boundary (4 polylines × 2 sides)              │
│                                                                      │
│   ┌─────────────────┐  ┌─────────────────┐  ┌──────────────────────┐ │
│   │ Qt6 / C++       │  │ Avalonia / C#   │  │ PySide6 / matplotlib │ │
│   │ frontend_qt     │  │ frontend_avalonia│ │ frontend_python      │ │
│   └─────────────────┘  └─────────────────┘  └──────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

Project is **Windows-first**. CMake, `.csproj`, and the Python loader
all handle Windows conventions (DLL next to the .exe, MSVC
`Release\`/`Debug\` config subdirectories).

---

## What it computes

```
Forward kinematics  (base at (base_x, base_y), θ in radians):
    joint1 = (base_x, base_y)
    joint2 = (base_x + l1·cos θ1,            base_y + l1·sin θ1)
    end    = (joint2.x + l2·cos(θ1+θ2),       joint2.y + l2·sin(θ1+θ2))

Workspace boundary  (4 polylines per side, sampled on the rectangle
    θ1 ∈ [θ1_min, θ1_max], θ2 ∈ [θ2_min, θ2_max]):
    curve A: θ2 = t2_lo (fixed), θ1 sweeps t1_min → t1_max
    curve B: θ2 = t2_hi (fixed), θ1 sweeps t1_min → t1_max
    curve C: θ1 = t1_min (fixed), θ2 sweeps t2_lo → t2_hi
    curve D: θ1 = t1_max (fixed), θ2 sweeps t2_lo → t2_hi

Python convention: compute twice — once for (t2_lo, t2_hi) = (θ2_min, 0)
giving the "-θ2" boundary (drawn red), and once for (0, θ2_max) giving
the "+θ2" boundary (drawn green). The two overlay to fill the reachable
area.

Defaults:
    l1 = l2 = 1.0
    θ1 ∈ [-π/2, π/2], θ2 ∈ [-π/2, π/2]
    100 samples per curve
    base = (0, 0)
```

---

## Directory layout

```
tlm\
├── core\                            # cross-language C++ library
│   ├── include\tlm_core.h
│   ├── src\tlm_core.cpp
│   ├── tools\smoke_test.cpp
│   └── CMakeLists.txt
├── frontend_python\
│   ├── tlm_core.py                  # ctypes bindings
│   ├── app_matplotlib.py            # drop-in for the reference script
│   ├── app_pyside6.py               # GUI: live sliders + workspace
│   └── requirements.txt
├── frontend_qt\
│   ├── main.cpp, MainWindow.{hpp,cpp}, Widgets.{hpp,cpp}
│   └── CMakeLists.txt               # runs windeployqt on Windows
├── frontend_avalonia\TlmAvalonia\
│   ├── TlmAvalonia.csproj           # copies tlm_core.dll next to .exe
│   ├── Native\{TlmCoreNative.cs, TlmSolver.cs}
│   ├── Models\{GridLineMarker.cs, BoundaryPolyline.cs}
│   ├── ViewModels\MainWindowViewModel.cs
│   ├── Views\MainWindow.{axaml,axaml.cs}
│   └── App.axaml{,.cs}, Program.cs, app.manifest
├── build_all.bat                    # Windows convenience build
├── build_all.sh                     # Linux/macOS convenience build
└── README.md
```

---

## 1. Building the core library

Requirements: **CMake 3.16+** and a C++17 compiler. **No external
dependencies** — pure C++.

### Windows (Visual Studio / MSVC)

```powershell
cd core
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
# tlm_core.dll lives at  core\build\Release\tlm_core.dll
```

Run the smoke test:

```powershell
cmake --build . --target tlm_core_smoke --config Release
.\Release\tlm_core_smoke.exe
```

Expected output (FK values agree with the Python formula to ≤1×10⁻⁷):

```
=== tlm_core 1.0.0 ===
Config: l1=1.000 l2=1.000  θ1∈[-1.5708, +1.5708]  θ2∈[-1.5708, +1.5708]
--- θ1=0, θ2=0           end=(+2.0000000, +0.0000000)  ...
--- θ1=π/4, θ2=π/4       end=(+0.7071068, +1.7071068)  ...
--- θ1=π/2, θ2=0         end=(+0.0000000, +2.0000000)  ...
--- θ1=0, θ2=π/2         end=(+1.0000000, +1.0000000)  ...
--- θ1=-π/4, θ2=π/3      end=(+1.6730326, -0.4482877)  ...
--- θ1=π/2, θ2=π/2       end=(-1.0000000, +1.0000000)  ...
Workspace boundary check (-θ2 side, t2 ∈ [-π/2, 0]):
    samples_per_curve = 100  (expect 100)
    curve A sample 99: (+1.0000, +1.0000)   (= end at θ1=+π/2, θ2=-π/2)
ALL OK.
```

### Linux / macOS

```bash
cd core
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
cmake --build . --target tlm_core_smoke -j
./tlm_core_smoke
```

---

## 2. Qt6 (C++) frontend

Requirements: **Qt 6.2+** (built with the same toolchain you use for
the core), CMake 3.16+.

### Windows

```powershell
cd frontend_qt
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 ^
         -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build . --config Release
.\Release\tlm_qt.exe
```

`windeployqt` runs automatically after the build, so all Qt DLLs end
up next to `tlm_qt.exe`. The core's `tlm_core.dll` is placed in the
same directory.

### Linux / macOS

```bash
cd frontend_qt
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j
./tlm_qt
```

The GUI is a 2-panel layout:
- **Left** — parameter editor (link lengths l1/l2, joint-angle limits,
  joint-angle sliders with live value labels, end-effector readout),
  plus Reset / Save PNG buttons.
- **Right** — equal-aspect 2D plot showing the workspace boundary
  (red = -θ2 side, green = +θ2 side), the arm pose, and an end-
  effector marker with a legend.

Drag the joint-angle sliders and the arm updates in real time. FK
recomputes in microseconds, and the workspace is re-traced on every
tick of the slider — no debouncing needed. Editing a limit preserves
the current joint angles where possible, so the arm doesn't snap to a
new pose unexpectedly.

---

## 3. Avalonia (C#) frontend

Requirements: **.NET 8.0 SDK** + internet for the first NuGet restore.

```powershell
# 1) build the core (above) so tlm_core.dll exists at
#    core\build\Release\tlm_core.dll

# 2) build + run the Avalonia frontend
cd frontend_avalonia\TlmAvalonia
dotnet run -c Release
```

The csproj automatically copies `tlm_core.dll` from
`..\..\core\build\Release\` (and `..\..\core\build\` for single-config
generators) next to the .exe in `bin\Release\net8.0\`.

---

## 4. Python frontend

Requirements: **Python 3.10+**, NumPy, matplotlib, PySide6.

```powershell
# build the core first as above, then:
cd frontend_python
pip install -r requirements.txt

python app_matplotlib.py   # drop-in: matplotlib Slider + Reset/Save (same UX as reference)
python app_pyside6.py      # GUI: native Qt sliders + workspace
```

The Python loader searches `..\core\build\`, `..\core\build\Release\`,
and `..\core\build\Debug\` automatically. Override with
`set TLM_CORE_LIB=path\to\tlm_core.dll`.

---

## Algorithm notes

1. **Pure-function FK**. Forward kinematics is closed-form and has no
   integration or iteration — it's three trigonometric evaluations.
   The core implements it as a pure function and exposes it via a
   `TlmPose` out-parameter.

2. **Workspace boundary**. The 2-link reachable area, restricted to a
   rectangular joint-angle window, is bounded by the four edges of
   that window: θ2 fixed at each extreme while θ1 sweeps, and θ1 fixed
   at each extreme while θ2 sweeps. The Python reference traces this
   twice — once for `θ2 ∈ [θ2_min, 0]` (red) and once for
   `θ2 ∈ [0, θ2_max]` (green) — so the two halves overlay into the
   familiar Pac-Man-shaped reachable region for symmetric limits.

3. **Sample count**. The reference uses `np.linspace(t_min, t_max,
   100)` for each curve — an inclusive linear sweep with 100 samples.
   The core uses the same convention; the smoke test verifies the
   endpoints land exactly where the Python formula says they should.

---

## C ABI reference (`core\include\tlm_core.h`)

```c
typedef struct TlmRobotConfig {
    double l1, l2;
    double theta1_min, theta1_max;
    double theta2_min, theta2_max;
    double base_x, base_y;
} TlmRobotConfig;

typedef struct TlmPose {
    double base_x,   base_y;
    double joint2_x, joint2_y;
    double end_x,    end_y;
} TlmPose;

typedef struct TlmWorkspace TlmWorkspace;

void tlm_core_default_config(TlmRobotConfig*);
int  tlm_core_forward_kinematics(const TlmRobotConfig*,
                                 double theta1, double theta2, TlmPose*);

TlmWorkspace* tlm_core_compute_workspace(const TlmRobotConfig*,
                                         double t2_lo, double t2_hi,
                                         int32_t samples_per_curve);
/* … 8 per-curve accessors (a/b/c/d × x/y) … */
```

All numeric fields are `double`; sizes/indices are `int32_t`.
