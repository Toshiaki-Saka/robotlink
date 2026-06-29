"""
tlm_core.py — Python bindings for the C++ two-link manipulator core.

Loads libtlm_core.so / .dylib / tlm_core.dll via ctypes.

Search order:
  1. $TLM_CORE_LIB
  2. ../core/build/{Release,Debug,}      (Windows MSVC multi-config)
  3. ctypes.util.find_library('tlm_core')
"""

from __future__ import annotations

import ctypes
import ctypes.util
import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import List, Tuple

import numpy as np


def _candidate_paths() -> List[Path]:
    here = Path(__file__).resolve().parent
    if sys.platform.startswith("win"):
        names = ["tlm_core.dll"]
    elif sys.platform == "darwin":
        names = ["libtlm_core.dylib"]
    else:
        names = ["libtlm_core.so"]
    out: List[Path] = []
    env = os.environ.get("TLM_CORE_LIB")
    if env:
        out.append(Path(env))
    base_dirs = [
        (here / ".." / "core" / "build").resolve(),
        (here / ".." / "core" / "build" / "Release").resolve(),
        (here / ".." / "core" / "build" / "Debug").resolve(),
        here, here / "..", here.parent,
    ]
    for d in base_dirs:
        for n in names:
            out.append(d / n)
    return out


def _load_library() -> ctypes.CDLL:
    for p in _candidate_paths():
        if p.is_file():
            return ctypes.CDLL(str(p))
    found = ctypes.util.find_library("tlm_core")
    if found:
        return ctypes.CDLL(found)
    raise OSError(
        "Could not locate the tlm_core shared library. "
        "Build it first (see core/CMakeLists.txt) or set TLM_CORE_LIB."
    )


_lib = _load_library()


class _TlmConfig(ctypes.Structure):
    _fields_ = [
        ("l1",         ctypes.c_double),
        ("l2",         ctypes.c_double),
        ("theta1_min", ctypes.c_double),
        ("theta1_max", ctypes.c_double),
        ("theta2_min", ctypes.c_double),
        ("theta2_max", ctypes.c_double),
        ("base_x",     ctypes.c_double),
        ("base_y",     ctypes.c_double),
    ]


class _TlmPose(ctypes.Structure):
    _fields_ = [
        ("base_x",   ctypes.c_double),
        ("base_y",   ctypes.c_double),
        ("joint2_x", ctypes.c_double),
        ("joint2_y", ctypes.c_double),
        ("end_x",    ctypes.c_double),
        ("end_y",    ctypes.c_double),
    ]


_lib.tlm_core_version.restype = ctypes.c_char_p
_lib.tlm_core_version.argtypes = []

_lib.tlm_core_default_config.restype = None
_lib.tlm_core_default_config.argtypes = [ctypes.POINTER(_TlmConfig)]

_lib.tlm_core_forward_kinematics.restype = ctypes.c_int32
_lib.tlm_core_forward_kinematics.argtypes = [
    ctypes.POINTER(_TlmConfig),
    ctypes.c_double, ctypes.c_double,
    ctypes.POINTER(_TlmPose),
]

_lib.tlm_core_compute_workspace.restype = ctypes.c_void_p
_lib.tlm_core_compute_workspace.argtypes = [
    ctypes.POINTER(_TlmConfig),
    ctypes.c_double, ctypes.c_double,
    ctypes.c_int32,
]
_lib.tlm_core_free_workspace.restype = None
_lib.tlm_core_free_workspace.argtypes = [ctypes.c_void_p]
_lib.tlm_core_ws_samples_per_curve.restype = ctypes.c_int32
_lib.tlm_core_ws_samples_per_curve.argtypes = [ctypes.c_void_p]
for _suffix in ("a_x", "a_y", "b_x", "b_y", "c_x", "c_y", "d_x", "d_y"):
    fn = getattr(_lib, f"tlm_core_ws_copy_{_suffix}")
    fn.restype  = ctypes.c_int32
    fn.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_double), ctypes.c_int32]


def version() -> str:
    return _lib.tlm_core_version().decode("utf-8")


@dataclass
class TlmRobotConfig:
    l1:         float = 1.0
    l2:         float = 1.0
    theta1_min: float = -math.pi / 2
    theta1_max: float =  math.pi / 2
    theta2_min: float = -math.pi / 2
    theta2_max: float =  math.pi / 2
    base_x:     float = 0.0
    base_y:     float = 0.0

    @classmethod
    def default(cls) -> "TlmRobotConfig":
        c = _TlmConfig(); _lib.tlm_core_default_config(ctypes.byref(c))
        return cls(l1=c.l1, l2=c.l2,
                   theta1_min=c.theta1_min, theta1_max=c.theta1_max,
                   theta2_min=c.theta2_min, theta2_max=c.theta2_max,
                   base_x=c.base_x, base_y=c.base_y)

    def _to_c(self) -> _TlmConfig:
        return _TlmConfig(
            l1=self.l1, l2=self.l2,
            theta1_min=self.theta1_min, theta1_max=self.theta1_max,
            theta2_min=self.theta2_min, theta2_max=self.theta2_max,
            base_x=self.base_x, base_y=self.base_y,
        )


@dataclass
class Pose:
    base:   Tuple[float, float]
    joint2: Tuple[float, float]
    end:    Tuple[float, float]


def forward_kinematics(cfg: TlmRobotConfig, theta1: float, theta2: float) -> Pose:
    cc = cfg._to_c()
    out = _TlmPose()
    ok = _lib.tlm_core_forward_kinematics(
        ctypes.byref(cc), ctypes.c_double(theta1), ctypes.c_double(theta2),
        ctypes.byref(out))
    if not ok:
        raise ValueError("tlm_core_forward_kinematics rejected the inputs")
    return Pose(
        base   =(out.base_x,   out.base_y),
        joint2 =(out.joint2_x, out.joint2_y),
        end    =(out.end_x,    out.end_y),
    )


@dataclass
class WorkspaceSide:
    """4 boundary curves for one (t2_lo, t2_hi) range."""
    a_x: np.ndarray; a_y: np.ndarray   # θ2=t2_lo, θ1 sweeps
    b_x: np.ndarray; b_y: np.ndarray   # θ2=t2_hi, θ1 sweeps
    c_x: np.ndarray; c_y: np.ndarray   # θ1=t1_min, θ2 sweeps
    d_x: np.ndarray; d_y: np.ndarray   # θ1=t1_max, θ2 sweeps


def compute_workspace(cfg: TlmRobotConfig,
                      t2_lo: float, t2_hi: float,
                      samples_per_curve: int = 100) -> WorkspaceSide:
    cc = cfg._to_c()
    handle = _lib.tlm_core_compute_workspace(
        ctypes.byref(cc),
        ctypes.c_double(t2_lo), ctypes.c_double(t2_hi),
        ctypes.c_int32(samples_per_curve))
    if not handle:
        raise ValueError("tlm_core_compute_workspace rejected the inputs")
    try:
        n = _lib.tlm_core_ws_samples_per_curve(handle)
        def _c(suffix):
            buf = (ctypes.c_double * n)()
            getattr(_lib, f"tlm_core_ws_copy_{suffix}")(handle, buf, n)
            return np.frombuffer(buf, dtype=np.float64).copy()
        return WorkspaceSide(
            a_x=_c("a_x"), a_y=_c("a_y"),
            b_x=_c("b_x"), b_y=_c("b_y"),
            c_x=_c("c_x"), c_y=_c("c_y"),
            d_x=_c("d_x"), d_y=_c("d_y"),
        )
    finally:
        _lib.tlm_core_free_workspace(handle)


__all__ = [
    "version", "TlmRobotConfig", "Pose", "WorkspaceSide",
    "forward_kinematics", "compute_workspace",
]
