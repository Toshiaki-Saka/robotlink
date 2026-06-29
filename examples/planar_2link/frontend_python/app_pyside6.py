"""
app_pyside6.py — PySide6 GUI for the two-link manipulator demo.

Layout:
  left   : robot config (link lengths, joint-angle limits) + ω sliders
  centre : live workspace plot with arm pose overlay
  bottom : Reset / Save PNG / status
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

import numpy as np
from PySide6.QtCore import Qt
from PySide6.QtGui  import QFontDatabase
from PySide6.QtWidgets import (
    QApplication, QDoubleSpinBox, QFileDialog, QFormLayout, QGroupBox,
    QHBoxLayout, QLabel, QMainWindow, QPushButton, QSlider, QSplitter,
    QStatusBar, QVBoxLayout, QWidget,
)

from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg as FigureCanvas
from matplotlib.figure import Figure

import tlm_core as tc


# Sliders use integer ticks; this resolution gives 0.001 rad steps over a
# default ±π/2 range — plenty fine for live drag-and-update.
_TICKS = 1000


class TlmCanvas(FigureCanvas):
    def __init__(self) -> None:
        self.figure = Figure(figsize=(7, 7))
        super().__init__(self.figure)
        self.ax = self.figure.add_subplot()

    def show_pose(self, cfg: tc.TlmRobotConfig,
                   pose: tc.Pose,
                   ws_minus: tc.WorkspaceSide,
                   ws_plus:  tc.WorkspaceSide) -> None:
        ax = self.ax
        ax.cla()
        # Workspace boundaries (red = -θ2 side, green = +θ2 side)
        ax.plot(ws_minus.a_x, ws_minus.a_y, "r", label="Workspace boundary (-theta2)")
        ax.plot(ws_minus.b_x, ws_minus.b_y, "r")
        ax.plot(ws_minus.c_x, ws_minus.c_y, "r")
        ax.plot(ws_minus.d_x, ws_minus.d_y, "r")
        ax.plot(ws_plus.a_x,  ws_plus.a_y,  "g", label="Workspace boundary (+theta2)")
        ax.plot(ws_plus.b_x,  ws_plus.b_y,  "g")
        ax.plot(ws_plus.c_x,  ws_plus.c_y,  "g")
        ax.plot(ws_plus.d_x,  ws_plus.d_y,  "g")

        # Arm (base → joint2 → end), with markers at each node
        xs = [pose.base[0], pose.joint2[0], pose.end[0]]
        ys = [pose.base[1], pose.joint2[1], pose.end[1]]
        ax.plot(xs, ys, "o-", lw=3, label="Manipulator")
        ax.plot([pose.end[0]], [pose.end[1]], "bo", label="End Effector")

        width = (cfg.l1 + cfg.l2) * 1.1
        ax.set_xlim(-width, width); ax.set_ylim(-width, width)
        ax.set_aspect("equal")
        ax.set_xlabel("X"); ax.set_ylabel("Y")
        ax.set_title("Two-Link Manipulator and Workspace")
        ax.grid(True); ax.legend(loc="upper right", fontsize=8)
        self.figure.tight_layout()
        self.draw_idle()


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(f"Two-link manipulator (Python + PySide6) — {tc.version()}")
        self.resize(1180, 760)
        self._cfg = tc.TlmRobotConfig.default()
        self._build_ui()
        self._refresh()

    # ---- factory ----
    def _spin(self, lo, hi, val, step, decimals=4) -> QDoubleSpinBox:
        b = QDoubleSpinBox(); b.setRange(lo, hi); b.setSingleStep(step)
        b.setDecimals(decimals); b.setValue(val); b.setMinimumWidth(110)
        return b

    def _build_ui(self) -> None:
        central = QWidget(); self.setCentralWidget(central)

        # --- left: parameters ---
        left = QWidget(); ll = QVBoxLayout(left)
        ll.setContentsMargins(6, 6, 6, 6); ll.setSpacing(6)

        links = QGroupBox("Link lengths"); lf = QFormLayout(links)
        self.ed_l1 = self._spin(0.001, 100.0, self._cfg.l1, 0.05, 4)
        self.ed_l2 = self._spin(0.001, 100.0, self._cfg.l2, 0.05, 4)
        lf.addRow("l1 [m]", self.ed_l1); lf.addRow("l2 [m]", self.ed_l2)
        ll.addWidget(links)

        lim = QGroupBox("Joint-angle limits [rad]"); lif = QFormLayout(lim)
        # The default ±π/2 limits are inherited; allow editing them.
        self.ed_t1_min = self._spin(-math.pi, math.pi, self._cfg.theta1_min, 0.05, 4)
        self.ed_t1_max = self._spin(-math.pi, math.pi, self._cfg.theta1_max, 0.05, 4)
        self.ed_t2_min = self._spin(-math.pi, math.pi, self._cfg.theta2_min, 0.05, 4)
        self.ed_t2_max = self._spin(-math.pi, math.pi, self._cfg.theta2_max, 0.05, 4)
        lif.addRow("theta1_min", self.ed_t1_min)
        lif.addRow("theta1_max", self.ed_t1_max)
        lif.addRow("theta2_min", self.ed_t2_min)
        lif.addRow("theta2_max", self.ed_t2_max)
        ll.addWidget(lim)

        ang = QGroupBox("Joint angles"); af = QVBoxLayout(ang); af.setSpacing(2)
        # Theta1 slider with live value label
        af.addWidget(QLabel("theta1 [rad]"))
        row1 = QHBoxLayout()
        self.sl_t1 = QSlider(Qt.Orientation.Horizontal)
        self.sl_t1.setMinimum(0); self.sl_t1.setMaximum(_TICKS)
        self.lbl_t1 = QLabel("0.000"); self.lbl_t1.setMinimumWidth(50)
        self.lbl_t1.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        row1.addWidget(self.sl_t1, 1); row1.addWidget(self.lbl_t1)
        af.addLayout(row1)
        # Theta2
        af.addWidget(QLabel("theta2 [rad]"))
        row2 = QHBoxLayout()
        self.sl_t2 = QSlider(Qt.Orientation.Horizontal)
        self.sl_t2.setMinimum(0); self.sl_t2.setMaximum(_TICKS)
        self.lbl_t2 = QLabel("0.000"); self.lbl_t2.setMinimumWidth(50)
        self.lbl_t2.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        row2.addWidget(self.sl_t2, 1); row2.addWidget(self.lbl_t2)
        af.addLayout(row2)
        ll.addWidget(ang)

        # Initial slider positions = θ=0 (mapped through the current limits).
        self._set_slider_from_angle(self.sl_t1, 0.0, self._cfg.theta1_min, self._cfg.theta1_max)
        self._set_slider_from_angle(self.sl_t2, 0.0, self._cfg.theta2_min, self._cfg.theta2_max)

        # End-effector status
        out = QGroupBox("End effector"); of = QFormLayout(out)
        self.lbl_xend = QLabel("0.000"); self.lbl_xend.setFont(
            QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        self.lbl_yend = QLabel("0.000"); self.lbl_yend.setFont(
            QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        of.addRow("x_end", self.lbl_xend); of.addRow("y_end", self.lbl_yend)
        ll.addWidget(out)

        # Buttons
        btns = QHBoxLayout()
        self.reset_btn = QPushButton("Reset")
        self.save_btn  = QPushButton("Save PNG…")
        btns.addWidget(self.reset_btn); btns.addWidget(self.save_btn)
        ll.addLayout(btns)
        ll.addStretch(1)

        # --- right: plot ---
        self.canvas = TlmCanvas()

        # --- assemble ---
        split = QSplitter(Qt.Orientation.Horizontal)
        split.addWidget(left); split.addWidget(self.canvas)
        split.setSizes([330, 850])
        split.setStretchFactor(0, 0); split.setStretchFactor(1, 1)
        outer = QVBoxLayout(central); outer.setContentsMargins(0, 0, 0, 0)
        outer.addWidget(split, 1)

        self.status_lbl = QLabel("Ready")
        self.status_lbl.setFont(QFontDatabase.systemFont(QFontDatabase.SystemFont.FixedFont))
        sb = QStatusBar(); sb.addWidget(self.status_lbl); self.setStatusBar(sb)

        # Wire up
        for w in (self.ed_l1, self.ed_l2,
                  self.ed_t1_min, self.ed_t1_max,
                  self.ed_t2_min, self.ed_t2_max):
            w.valueChanged.connect(lambda _v: self._on_config_changed())
        # Sliders fire on every drag tick — no debounce needed here, FK is
        # cheap and the C++ workspace recomputes in microseconds.
        self.sl_t1.valueChanged.connect(self._refresh)
        self.sl_t2.valueChanged.connect(self._refresh)
        self.reset_btn.clicked.connect(self._on_reset)
        self.save_btn.clicked.connect(self._on_save)

    # ---- helpers ----
    @staticmethod
    def _set_slider_from_angle(sl: QSlider, angle: float, lo: float, hi: float) -> None:
        if hi <= lo: sl.setValue(0); return
        f = (angle - lo) / (hi - lo)
        f = max(0.0, min(1.0, f))
        sl.setValue(int(round(f * _TICKS)))

    @staticmethod
    def _slider_angle(sl: QSlider, lo: float, hi: float) -> float:
        f = sl.value() / _TICKS
        return lo + f * (hi - lo)

    def _current_cfg(self) -> tc.TlmRobotConfig:
        return tc.TlmRobotConfig(
            l1=self.ed_l1.value(), l2=self.ed_l2.value(),
            theta1_min=self.ed_t1_min.value(), theta1_max=self.ed_t1_max.value(),
            theta2_min=self.ed_t2_min.value(), theta2_max=self.ed_t2_max.value(),
        )

    def _on_config_changed(self) -> None:
        new_cfg = self._current_cfg()
        # Preserve the current angles where possible — read first, then
        # remap to the new range so the arm doesn't snap to a new pose.
        t1 = self._slider_angle(self.sl_t1, self._cfg.theta1_min, self._cfg.theta1_max)
        t2 = self._slider_angle(self.sl_t2, self._cfg.theta2_min, self._cfg.theta2_max)
        self._cfg = new_cfg
        self._set_slider_from_angle(self.sl_t1, t1, new_cfg.theta1_min, new_cfg.theta1_max)
        self._set_slider_from_angle(self.sl_t2, t2, new_cfg.theta2_min, new_cfg.theta2_max)
        self._refresh()

    def _refresh(self, *_args) -> None:
        cfg = self._current_cfg()
        self._cfg = cfg
        t1 = self._slider_angle(self.sl_t1, cfg.theta1_min, cfg.theta1_max)
        t2 = self._slider_angle(self.sl_t2, cfg.theta2_min, cfg.theta2_max)
        try:
            pose = tc.forward_kinematics(cfg, t1, t2)
            ws_minus = tc.compute_workspace(cfg, cfg.theta2_min, 0.0, 100)
            ws_plus  = tc.compute_workspace(cfg, 0.0, cfg.theta2_max, 100)
        except Exception as exc:
            self.status_lbl.setText(f"Bad config: {exc}")
            return
        self.canvas.show_pose(cfg, pose, ws_minus, ws_plus)
        self.lbl_t1.setText(f"{t1:+.3f}")
        self.lbl_t2.setText(f"{t2:+.3f}")
        self.lbl_xend.setText(f"{pose.end[0]:+.3f}")
        self.lbl_yend.setText(f"{pose.end[1]:+.3f}")
        self.status_lbl.setText(
            f"theta1 = {t1:+.3f} rad, theta2 = {t2:+.3f} rad, "
            f"end = ({pose.end[0]:+.3f}, {pose.end[1]:+.3f})"
        )

    def _on_reset(self) -> None:
        d = tc.TlmRobotConfig.default()
        self.ed_l1.setValue(d.l1); self.ed_l2.setValue(d.l2)
        self.ed_t1_min.setValue(d.theta1_min); self.ed_t1_max.setValue(d.theta1_max)
        self.ed_t2_min.setValue(d.theta2_min); self.ed_t2_max.setValue(d.theta2_max)
        self._cfg = d
        self._set_slider_from_angle(self.sl_t1, 0.0, d.theta1_min, d.theta1_max)
        self._set_slider_from_angle(self.sl_t2, 0.0, d.theta2_min, d.theta2_max)
        self._refresh()

    def _on_save(self) -> None:
        path, _ = QFileDialog.getSaveFileName(
            self, "Save plot",
            str(Path.cwd() / "two_link_workspace.png"),
            "PNG (*.png);;All files (*)")
        if not path: return
        self.canvas.figure.savefig(path, dpi=150, bbox_inches="tight")
        self.status_lbl.setText(f"Saved: {path}")


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow(); w.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
