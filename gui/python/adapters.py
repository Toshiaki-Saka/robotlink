"""
adapters.py — run the absorbed planar 2-link arm (former `tlm` core) through its
C ABI and normalise into a common :class:`RunResult`.

`planar_2link` is the simplified planar sibling of RobotLink's spatial 3-DOF arm:
it provides forward kinematics and reachable-workspace boundary curves for a
2-link planar manipulator, useful as an entry-level example.

``libloader`` primes ``TLM_CORE_LIB`` before the binding loads.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Callable, Dict, List

import numpy as np

import libloader  # noqa: F401


@dataclass
class Trace:
    label: str
    x: np.ndarray
    y: np.ndarray
    style: str = "-"


@dataclass
class Plot:
    title: str
    xlabel: str
    ylabel: str
    traces: List[Trace] = field(default_factory=list)
    equal_aspect: bool = False


@dataclass
class RunResult:
    name: str
    description: str
    plots: List[Plot] = field(default_factory=list)
    metrics: Dict[str, float] = field(default_factory=dict)


def run_planar_2link() -> RunResult:
    from bindings import tlm_core
    cfg = tlm_core.TlmRobotConfig()

    # A few arm poses sweeping theta1, elbow fixed.
    pose_traces: List[Trace] = []
    for i, th1 in enumerate(np.linspace(cfg.theta1_min, cfg.theta1_max, 5)):
        p = tlm_core.forward_kinematics(cfg, float(th1), math.pi / 4)
        xs = np.array([p.base[0], p.joint2[0], p.end[0]])
        ys = np.array([p.base[1], p.joint2[1], p.end[1]])
        pose_traces.append(Trace(f"pose {i+1}", xs, ys))

    # Reachable workspace boundary.
    ws = tlm_core.compute_workspace(cfg, cfg.theta2_min, cfg.theta2_max)
    ws_traces = [
        Trace("θ2=lo", ws.a_x, ws.a_y, "-"),
        Trace("θ2=hi", ws.b_x, ws.b_y, "-"),
        Trace("θ1=min", ws.c_x, ws.c_y, "-"),
        Trace("θ1=max", ws.d_x, ws.d_y, "-"),
    ]

    reach = float(np.max(np.hypot(np.concatenate([ws.a_x, ws.b_x]),
                                  np.concatenate([ws.a_y, ws.b_y]))))
    return RunResult(
        "planar_2link", "Forward kinematics and reachable workspace of a planar 2-link arm",
        plots=[
            Plot("Arm poses (θ1 sweep)", "x", "y", pose_traces, equal_aspect=True),
            Plot("Reachable workspace boundary", "x", "y", ws_traces, equal_aspect=True),
        ],
        metrics={"max reach": reach, "l1+l2": cfg.l1 + cfg.l2},
    )


EXAMPLES: Dict[str, Callable[[], RunResult]] = {
    "planar_2link": run_planar_2link,
}


def run(name: str) -> RunResult:
    return EXAMPLES[name]()


def run_all() -> List[RunResult]:
    out: List[RunResult] = []
    for name, fn in EXAMPLES.items():
        try:
            out.append(fn())
        except Exception as exc:
            out.append(RunResult(name, f"error: {exc}"))
    return out
