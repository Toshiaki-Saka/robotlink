"""
3-DOF Robot Arm Simulation Visualizer
GUI: tkinter + matplotlib embedded panels
Usage:
    python visualizer.py [path/to/sim_results.csv]
"""
import os
import sys
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
from matplotlib.animation import FuncAnimation
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

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


class App:
    def __init__(self, root: tk.Tk, csv_path: str | None = None):
        self.root   = root
        self.root.title("3-DOF Robot Arm — Simulation Visualizer")
        self.root.geometry("1100x780")
        self.df     = None
        self._anim  = None
        self._build_menu()
        self._build_toolbar()
        self._build_notebook()
        self._build_statusbar()
        if csv_path and os.path.isfile(csv_path):
            self._load(csv_path)

    # ── Layout ──────────────────────────────────────────────────────────────

    def _build_menu(self):
        menubar = tk.Menu(self.root)
        file_m = tk.Menu(menubar, tearoff=0)
        file_m.add_command(label="Open CSV…", command=self._open_csv)
        file_m.add_separator()
        file_m.add_command(label="Quit", command=self._on_close)
        menubar.add_cascade(label="File", menu=file_m)
        self.root.config(menu=menubar)
        self.root.protocol("WM_DELETE_WINDOW", self._on_close)

    def _build_toolbar(self):
        tb = ttk.Frame(self.root, padding=4)
        tb.pack(side=tk.TOP, fill=tk.X)

        ttk.Button(tb, text="Open CSV", command=self._open_csv).pack(side=tk.LEFT, padx=2)
        ttk.Button(tb, text="Run Simulation", command=self._run_sim).pack(side=tk.LEFT, padx=2)
        ttk.Separator(tb, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=6)
        ttk.Button(tb, text="Plot All", command=self._plot_all).pack(side=tk.LEFT, padx=2)
        ttk.Button(tb, text="Play 3-D Animation", command=self._start_animation).pack(side=tk.LEFT, padx=2)
        ttk.Button(tb, text="Stop Animation", command=self._stop_animation).pack(side=tk.LEFT, padx=2)

    def _build_notebook(self):
        # Horizontal split: graph tabs on the left, animation always visible on the right
        self.paned = ttk.PanedWindow(self.root, orient=tk.HORIZONTAL)
        self.paned.pack(fill=tk.BOTH, expand=True, padx=4, pady=2)

        left = ttk.Frame(self.paned)
        self.paned.add(left, weight=1)
        self.nb = ttk.Notebook(left)
        self.nb.pack(fill=tk.BOTH, expand=True)
        self.tab_angles  = self._make_tab("Joint Angles")
        self.tab_torques = self._make_tab("Torques")
        self.tab_errors  = self._make_tab("Errors")

        right = ttk.LabelFrame(self.paned, text="3-D Animation")
        self.paned.add(right, weight=1)
        self.tab_anim = right

    def _make_tab(self, title: str) -> ttk.Frame:
        frame = ttk.Frame(self.nb)
        self.nb.add(frame, text=title)
        return frame

    def _build_statusbar(self):
        self.status_var = tk.StringVar(value="No data loaded.")
        ttk.Label(self.root, textvariable=self.status_var,
                  relief=tk.SUNKEN, anchor=tk.W).pack(side=tk.BOTTOM, fill=tk.X)

    # ── Data loading ─────────────────────────────────────────────────────────

    def _open_csv(self):
        path = filedialog.askopenfilename(
            title="Open simulation results CSV",
            initialdir=os.path.join(ROOT_DIR, "output"),
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")])
        if path:
            self._load(path)

    def _load(self, path: str):
        try:
            self.df = pd.read_csv(path)
            self.status_var.set(f"Loaded: {path}  ({len(self.df)} rows)")
            self._plot_all()
            self.root.after(200, self._start_animation)
        except Exception as e:
            messagebox.showerror("Load error", str(e))

    # ── Simulation runner ────────────────────────────────────────────────────

    def _run_sim(self):
        binary = find_binary()
        if not binary:
            messagebox.showwarning("Binary not found",
                "Could not find robot_sim binary.\n"
                "Build with:\n  cmake -B build -S .\n  cmake --build build")
            return
        self.status_var.set("Running simulation…")
        self.root.update()

        def run():
            try:
                result = subprocess.run(
                    [binary, os.path.join(ROOT_DIR, "output")],
                    capture_output=True, text=True, timeout=120)
                if result.returncode == 0:
                    self.root.after(0, lambda: self._load(DEFAULT_CSV))
                else:
                    self.root.after(0, lambda: messagebox.showerror(
                        "Simulation error", result.stderr[-2000:]))
            except subprocess.TimeoutExpired:
                self.root.after(0, lambda: messagebox.showerror(
                    "Timeout", "Simulation exceeded 120 s."))
            except Exception as e:
                self.root.after(0, lambda: messagebox.showerror("Error", str(e)))

        threading.Thread(target=run, daemon=True).start()

    # ── Plotting ─────────────────────────────────────────────────────────────

    def _clear_tab(self, tab: ttk.Frame):
        for w in tab.winfo_children():
            w.destroy()

    def _embed_figure(self, fig: plt.Figure, tab: ttk.Frame):
        self._clear_tab(tab)
        canvas = FigureCanvasTkAgg(fig, master=tab)
        canvas.draw()
        toolbar = NavigationToolbar2Tk(canvas, tab)
        toolbar.update()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)

    def _plot_all(self):
        if self.df is None:
            return
        self._plot_angles()
        self._plot_torques()
        self._plot_errors()
        self._setup_animation_tab()

    def _plot_angles(self):
        df = self.df
        fig, axes = plt.subplots(3, 1, figsize=(9, 6), sharex=True)
        fig.suptitle("Joint Angles: Actual vs Desired")
        labels = ["q1 (shoulder yaw)", "q2 (shoulder pitch)", "q3 (elbow)"]
        for i, (ax, lbl) in enumerate(zip(axes, labels)):
            col_act = f"q{i+1}"
            col_des = f"qd{i+1}"
            ax.plot(df["t"], df[col_des], "--", alpha=0.7, label=f"{lbl} desired")
            ax.plot(df["t"], df[col_act], label=f"{lbl} actual")
            ax.set_ylabel("rad")
            ax.legend(fontsize=7, ncol=2)
            ax.grid(True, alpha=0.4)
        axes[-1].set_xlabel("time [s]")
        fig.tight_layout()
        self._embed_figure(fig, self.tab_angles)

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
        self._embed_figure(fig, self.tab_torques)

    def _plot_errors(self):
        df = self.df
        fig, axes = plt.subplots(2, 1, figsize=(9, 6), sharex=True)
        fig.suptitle("Tracking Errors")

        for i in range(3):
            axes[0].plot(df["t"], df[f"err{i+1}"] * (180 / np.pi),
                         label=f"e{i+1}")
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
        self._embed_figure(fig, self.tab_errors)

    # ── 3-D animation ────────────────────────────────────────────────────────

    def _setup_animation_tab(self):
        self._clear_tab(self.tab_anim)
        self._anim = None
        self._anim_df = self.df

        fig = plt.figure(figsize=(8, 6))
        self._anim_fig = fig
        ax = fig.add_subplot(111, projection="3d")
        self._anim_ax = ax

        reach = 0.9  # L2 + L3
        ax.set_xlim(-reach, reach); ax.set_ylim(-reach, reach); ax.set_zlim(-reach * 0.6, reach)
        ax.set_xlabel("X"); ax.set_ylabel("Y"); ax.set_zlabel("Z")
        ax.set_title("3-DOF Arm Trajectory Tracking")

        self._arm_line,    = ax.plot([], [], [], "o-", lw=4, color="steelblue", markersize=7)
        self._traj_actual, = ax.plot([], [], [], "-",  lw=1, color="tab:orange", label="actual hand path")
        self._traj_des,    = ax.plot([], [], [], "--", lw=1, color="tab:green",  label="desired hand path")
        self._time_text    = ax.text2D(0.02, 0.95, "", transform=ax.transAxes)
        ax.legend(loc="upper right", fontsize=8)

        xs = np.linspace(-reach, reach, 5); ys = np.linspace(-reach, reach, 5)
        Xg, Yg = np.meshgrid(xs, ys)
        ax.plot_wireframe(Xg, Yg, np.zeros_like(Xg), color="lightgray", linewidth=0.3)

        fig.tight_layout()
        canvas = FigureCanvasTkAgg(fig, master=self.tab_anim)
        canvas.draw()
        toolbar = NavigationToolbar2Tk(canvas, self.tab_anim)
        toolbar.update()
        canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        self._anim_canvas = canvas

    def _start_animation(self):
        if self.df is None:
            messagebox.showinfo("No data", "Load a CSV first.")
            return
        if self._anim is not None:
            return  # already running

        df = self._anim_df

        stride = max(1, int(0.03 / 0.005))  # ~30 fps at dt=0.005
        frames = range(0, len(df), stride)

        hand     = df[["hand_x",     "hand_y",     "hand_z"]].values
        hand_des = df[["hand_des_x", "hand_des_y", "hand_des_z"]].values
        t_vals   = df["t"].values

        # Reconstruct elbow from q values (L2=0.5)
        L2 = 0.5
        c1 = np.cos(df["q1"].values); s1 = np.sin(df["q1"].values)
        c2 = np.cos(df["q2"].values); s2 = np.sin(df["q2"].values)
        elbows = L2 * np.column_stack([c1 * c2, s1 * c2, -s2])

        def init():
            for line in [self._arm_line, self._traj_actual, self._traj_des]:
                line.set_data([], []); line.set_3d_properties([])
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
        self._anim_canvas.draw()

    def _stop_animation(self):
        if self._anim is not None:
            self._anim.event_source.stop()
            self._anim = None

    def _on_close(self):
        self._stop_animation()
        plt.close("all")
        self.root.destroy()


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CSV
    root = tk.Tk()
    app = App(root, csv_path if os.path.isfile(csv_path) else None)
    root.mainloop()


if __name__ == "__main__":
    main()
