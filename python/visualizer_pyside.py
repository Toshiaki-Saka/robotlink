"""
3-DOF Robot Arm Simulation Visualizer — PySide6 edition
Embeds matplotlib figures inside a QMainWindow; output is identical to good.png.

Usage:
    python visualizer_pyside.py [path/to/sim_results.csv]

Requirements:
    pip install PySide6 matplotlib numpy pandas
"""
import os
import sys
import subprocess
import threading

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("QtAgg")   # use the Qt6/PySide6 Agg backend
import matplotlib.pyplot as plt
from matplotlib.backends.backend_qtagg import FigureCanvasQTAgg, NavigationToolbar2QT
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D   # noqa: F401

from PySide6.QtCore    import Qt, Slot
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QSplitter,
    QTabWidget, QVBoxLayout, QLabel, QFileDialog,
    QMessageBox, QToolBar, QStatusBar,
)
from PySide6.QtGui import QAction

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_CSV = os.path.join(ROOT_DIR, "output", "sim_results.csv")
SIM_BIN_CANDIDATES = [
    os.path.join(ROOT_DIR, "build", "Release", "robot_sim.exe"),
    os.path.join(ROOT_DIR, "build", "robot_sim.exe"),
    os.path.join(ROOT_DIR, "build", "Debug",   "robot_sim.exe"),
    os.path.join(ROOT_DIR, "build", "robot_sim"),
]


def find_binary():
    for p in SIM_BIN_CANDIDATES:
        if os.path.isfile(p):
            return p
    return None


class EmbeddedFigure(QWidget):
    """A QWidget that holds a matplotlib Figure + NavigationToolbar."""
    def __init__(self, fig: plt.Figure, parent=None):
        super().__init__(parent)
        lay = QVBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.setSpacing(0)
        self.canvas  = FigureCanvasQTAgg(fig)
        self.toolbar = NavigationToolbar2QT(self.canvas, self)
        lay.addWidget(self.canvas)
        lay.addWidget(self.toolbar)

    def refresh(self):
        self.canvas.draw_idle()


class MainWindow(QMainWindow):
    def __init__(self, csv_path: str | None = None):
        super().__init__()
        self.setWindowTitle("3-DOF Robot Arm — Simulation Visualizer")
        self.resize(1100, 780)
        self.df: pd.DataFrame | None = None
        self._anim: FuncAnimation | None = None
        self._anim_fig: plt.Figure | None = None
        self._anim_canvas: FigureCanvasQTAgg | None = None

        self._build_toolbar()
        self._build_central()
        self._build_statusbar()

        if csv_path and os.path.isfile(csv_path):
            self._load(csv_path)

    # ── Layout ──────────────────────────────────────────────────────────────

    def _build_toolbar(self):
        tb: QToolBar = self.addToolBar("Main")
        tb.setMovable(False)

        def act(label, slot):
            a = QAction(label, self)
            a.triggered.connect(slot)
            tb.addAction(a)
            return a

        act("Open CSV",           self._open_csv)
        act("Run Simulation",     self._run_sim)
        tb.addSeparator()
        act("Plot All",           self._plot_all)
        act("Play 3-D Animation", self._start_animation)
        act("Stop Animation",     self._stop_animation)

    def _build_central(self):
        splitter = QSplitter(Qt.Orientation.Horizontal)

        # Left: tabbed chart area
        self._tabs = QTabWidget()
        self._tab_angles  = self._add_tab("Joint Angles")
        self._tab_torques = self._add_tab("Torques")
        self._tab_errors  = self._add_tab("Errors")
        splitter.addWidget(self._tabs)

        # Right: 3-D animation (always visible, wrapped in a labelled container)
        right = QWidget()
        rlay  = QVBoxLayout(right)
        rlay.setContentsMargins(2, 2, 2, 2)
        rlay.setSpacing(0)
        hdr = QLabel("3-D Animation")
        hdr.setAlignment(Qt.AlignmentFlag.AlignCenter)
        rlay.addWidget(hdr)
        self._anim_container = QWidget()
        self._anim_layout    = QVBoxLayout(self._anim_container)
        self._anim_layout.setContentsMargins(0, 0, 0, 0)
        rlay.addWidget(self._anim_container, 1)
        splitter.addWidget(right)

        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 1)
        self.setCentralWidget(splitter)

    def _add_tab(self, title: str) -> QWidget:
        w = QWidget()
        self._tabs.addTab(w, title)
        return w

    def _build_statusbar(self):
        self._status = QStatusBar()
        self.setStatusBar(self._status)
        self._status.showMessage("No data loaded.")

    # ── Data loading ─────────────────────────────────────────────────────────

    def _open_csv(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "Open simulation results CSV",
            os.path.join(ROOT_DIR, "output"),
            "CSV files (*.csv);;All files (*.*)")
        if path:
            self._load(path)

    def _load(self, path: str):
        try:
            self.df = pd.read_csv(path)
            self._status.showMessage(f"Loaded: {path}  ({len(self.df)} rows)")
            self._plot_all()
            # Delay animation start so the canvas is fully shown first
            from PySide6.QtCore import QTimer
            QTimer.singleShot(200, self._start_animation)
        except Exception as e:
            QMessageBox.critical(self, "Load error", str(e))

    # ── Simulation runner ────────────────────────────────────────────────────

    def _run_sim(self):
        binary = find_binary()
        if not binary:
            QMessageBox.warning(self, "Binary not found",
                "Could not find robot_sim binary.\n"
                "Build with:\n  cmake -B build -S .\n  cmake --build build")
            return
        self._status.showMessage("Running simulation…")

        def run():
            try:
                result = subprocess.run(
                    [binary, os.path.join(ROOT_DIR, "output")],
                    capture_output=True, text=True, timeout=120)
                if result.returncode == 0:
                    from PySide6.QtCore import QMetaObject, Qt
                    QMetaObject.invokeMethod(
                        self, "_load_default", Qt.ConnectionType.QueuedConnection)
                else:
                    from PySide6.QtCore import QMetaObject, Qt
                    err = result.stderr[-2000:]
                    QMetaObject.invokeMethod(
                        self, "_show_sim_error", Qt.ConnectionType.QueuedConnection,
                        err)
            except subprocess.TimeoutExpired:
                pass
            except Exception as ex:
                pass

        threading.Thread(target=run, daemon=True).start()

    @Slot()
    def _load_default(self):
        self._load(DEFAULT_CSV)

    @Slot(str)
    def _show_sim_error(self, msg: str):
        QMessageBox.critical(self, "Simulation error", msg)
        self._status.showMessage("Simulation failed.")

    # ── Plotting ─────────────────────────────────────────────────────────────

    @staticmethod
    def _clear_widget(w: QWidget):
        lay = w.layout()
        if lay:
            while lay.count():
                item = lay.takeAt(0)
                if item.widget():
                    item.widget().deleteLater()
        else:
            for child in w.findChildren(QWidget):
                child.deleteLater()

    def _embed(self, fig: plt.Figure, tab: QWidget):
        self._clear_widget(tab)
        ef = EmbeddedFigure(fig, tab)
        lay = QVBoxLayout(tab)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(ef)

    def _plot_all(self):
        if self.df is None:
            return
        self._plot_angles()
        self._plot_torques()
        self._plot_errors()
        self._setup_animation()

    def _plot_angles(self):
        df = self.df
        fig, axes = plt.subplots(3, 1, figsize=(9, 6), sharex=True)
        fig.suptitle("Joint Angles: Actual vs Desired")
        labels = ["q1 (shoulder yaw)", "q2 (shoulder pitch)", "q3 (elbow)"]
        for i, (ax, lbl) in enumerate(zip(axes, labels)):
            ax.plot(df["t"], df[f"qd{i+1}"], "--", alpha=0.7, label=f"{lbl} desired")
            ax.plot(df["t"], df[f"q{i+1}"],         label=f"{lbl} actual")
            ax.set_ylabel("rad")
            ax.legend(fontsize=7, ncol=2)
            ax.grid(True, alpha=0.4)
        axes[-1].set_xlabel("time [s]")
        fig.tight_layout()
        self._embed(fig, self._tab_angles)

    def _plot_torques(self):
        df = self.df
        fig, ax = plt.subplots(figsize=(9, 4))
        fig.suptitle("Joint Torques")
        for i in range(3):
            ax.plot(df["t"], df[f"tau{i+1}"], label=f"τ{i+1}")
        ax.set_xlabel("time [s]")
        ax.set_ylabel("torque [N·m]")
        ax.legend()
        ax.grid(True, alpha=0.4)
        fig.tight_layout()
        self._embed(fig, self._tab_torques)

    def _plot_errors(self):
        df = self.df
        fig, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
        fig.suptitle("Tracking Errors")

        for i in range(3):
            axes[0].plot(df["t"], df[f"err{i+1}"] * (180 / np.pi), label=f"e{i+1}")
        axes[0].set_ylabel("error [deg]")
        axes[0].legend()
        axes[0].grid(True, alpha=0.4)

        hand_err = np.linalg.norm(
            df[["hand_x", "hand_y", "hand_z"]].values -
            df[["hand_des_x", "hand_des_y", "hand_des_z"]].values,
            axis=1) * 1000
        axes[1].plot(df["t"], hand_err, color="crimson")
        axes[1].set_ylabel("hand position error [mm]")
        axes[1].set_xlabel("time [s]")
        axes[1].grid(True, alpha=0.4)

        steady = df["t"] >= 2.0
        axes[1].axvline(2.0, color="gray", linestyle=":", alpha=0.6)
        rms_ss = np.sqrt(np.mean(hand_err[steady.values] ** 2))
        axes[1].set_title(f"Steady-state hand RMS error: {rms_ss:.3f} mm")

        fig.tight_layout()
        self._embed(fig, self._tab_errors)

    # ── 3-D animation ────────────────────────────────────────────────────────

    def _setup_animation(self):
        self._clear_widget(self._anim_container)
        self._anim = None

        fig = plt.figure(figsize=(8, 6))
        self._anim_fig = fig
        ax = fig.add_subplot(111, projection="3d")
        self._anim_ax = ax

        reach = 0.9
        ax.set_xlim(-reach, reach)
        ax.set_ylim(-reach, reach)
        ax.set_zlim(-reach * 0.6, reach)
        ax.set_xlabel("X"); ax.set_ylabel("Y"); ax.set_zlabel("Z")
        ax.set_title("3-DOF Arm Trajectory Tracking")

        self._arm_line,    = ax.plot([], [], [], "o-", lw=4, color="steelblue", markersize=7)
        self._traj_actual, = ax.plot([], [], [], "-",  lw=1, color="tab:orange", label="actual hand path")
        self._traj_des,    = ax.plot([], [], [], "--", lw=1, color="tab:green",  label="desired hand path")
        self._time_text    = ax.text2D(0.02, 0.95, "", transform=ax.transAxes)
        ax.legend(loc="upper right", fontsize=8)

        xs = np.linspace(-reach, reach, 5)
        ys = np.linspace(-reach, reach, 5)
        Xg, Yg = np.meshgrid(xs, ys)
        ax.plot_wireframe(Xg, Yg, np.zeros_like(Xg), color="lightgray", linewidth=0.3)

        fig.tight_layout()
        ef = EmbeddedFigure(fig, self._anim_container)
        lay = QVBoxLayout(self._anim_container)
        lay.setContentsMargins(0, 0, 0, 0)
        lay.addWidget(ef)
        self._anim_canvas = ef.canvas

    def _start_animation(self):
        if self.df is None:
            QMessageBox.information(self, "No data", "Load a CSV first.")
            return
        if self._anim is not None:
            return

        df = self.df
        stride = max(1, int(0.03 / 0.005))
        frames = range(0, len(df), stride)

        hand     = df[["hand_x",     "hand_y",     "hand_z"]].values
        hand_des = df[["hand_des_x", "hand_des_y", "hand_des_z"]].values
        t_vals   = df["t"].values

        L2 = 0.5
        c1 = np.cos(df["q1"].values); s1 = np.sin(df["q1"].values)
        c2 = np.cos(df["q2"].values); s2 = np.sin(df["q2"].values)
        elbows = L2 * np.column_stack([c1 * c2, s1 * c2, -s2])

        def init():
            for line in [self._arm_line, self._traj_actual, self._traj_des]:
                line.set_data([], [])
                line.set_3d_properties([])
            self._time_text.set_text("")
            return self._arm_line, self._traj_actual, self._traj_des, self._time_text

        def update(i):
            elbow = elbows[i]; hnd = hand[i]
            self._arm_line.set_data([0, elbow[0], hnd[0]], [0, elbow[1], hnd[1]])
            self._arm_line.set_3d_properties([0, elbow[2], hnd[2]])
            self._traj_actual.set_data(hand[:i+1, 0], hand[:i+1, 1])
            self._traj_actual.set_3d_properties(hand[:i+1, 2])
            self._traj_des.set_data(hand_des[:i+1, 0], hand_des[:i+1, 1])
            self._traj_des.set_3d_properties(hand_des[:i+1, 2])
            self._time_text.set_text(f"t = {t_vals[i]:.2f} s")
            return self._arm_line, self._traj_actual, self._traj_des, self._time_text

        self._anim = FuncAnimation(
            self._anim_fig, update, frames=frames,
            init_func=init, interval=30, blit=False, repeat=True)
        if self._anim_canvas:
            self._anim_canvas.draw_idle()

    def _stop_animation(self):
        if self._anim is not None:
            self._anim.event_source.stop()
            self._anim = None

    def closeEvent(self, event):
        self._stop_animation()
        plt.close("all")
        super().closeEvent(event)


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CSV
    app = QApplication(sys.argv)
    win = MainWindow(csv_path if os.path.isfile(csv_path) else None)
    win.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
