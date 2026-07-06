"""
3-DOF Robot Arm Simulation Visualizer (matplotlib standalone)
Usage:
    python visualizer_matplotlib.py [path/to/sim_results.csv]
"""
import os
import sys

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_CSV = os.path.join(ROOT_DIR, "output", "sim_results.csv")


def plot_angles(df: pd.DataFrame, ax_list):
    labels = ["q1 (shoulder yaw)", "q2 (shoulder pitch)", "q3 (elbow)"]
    for i, (ax, lbl) in enumerate(zip(ax_list, labels)):
        ax.plot(df["t"], df[f"qd{i+1}"], "--", alpha=0.7, label=f"{lbl} desired")
        ax.plot(df["t"], df[f"q{i+1}"], label=f"{lbl} actual")
        ax.set_ylabel("rad")
        ax.legend(fontsize=7, ncol=2)
        ax.grid(True, alpha=0.4)
    ax_list[-1].set_xlabel("time [s]")


def plot_torques(df: pd.DataFrame, ax):
    for i in range(3):
        ax.plot(df["t"], df[f"tau{i+1}"], label=f"τ{i+1}")
    ax.set_xlabel("time [s]")
    ax.set_ylabel("torque [N·m]")
    ax.legend()
    ax.grid(True, alpha=0.4)


def plot_errors(df: pd.DataFrame, axes):
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


def plot_3d_trajectory(df: pd.DataFrame, ax):
    L2 = 0.5
    c1 = np.cos(df["q1"].values); s1 = np.sin(df["q1"].values)
    c2 = np.cos(df["q2"].values); s2 = np.sin(df["q2"].values)
    elbows = L2 * np.column_stack([c1 * c2, s1 * c2, -s2])

    hand     = df[["hand_x",     "hand_y",     "hand_z"]].values
    hand_des = df[["hand_des_x", "hand_des_y", "hand_des_z"]].values

    ax.plot(hand[:, 0],     hand[:, 1],     hand[:, 2],     "-",  lw=1.2,
            color="tab:orange", label="actual hand path")
    ax.plot(hand_des[:, 0], hand_des[:, 1], hand_des[:, 2], "--", lw=1.2,
            color="tab:green",  label="desired hand path")

    n = len(df)
    stride = max(1, n // 6)
    for k in range(0, n, stride):
        elbow = elbows[k]; hnd = hand[k]
        ax.plot([0, elbow[0], hnd[0]],
                [0, elbow[1], hnd[1]],
                [0, elbow[2], hnd[2]],
                "o-", lw=2, color="steelblue", markersize=4, alpha=0.4)

    reach = 0.9
    ax.set_xlim(-reach, reach); ax.set_ylim(-reach, reach); ax.set_zlim(-reach * 0.6, reach)
    ax.set_xlabel("X"); ax.set_ylabel("Y"); ax.set_zlabel("Z")
    ax.legend(fontsize=8)

    xs = np.linspace(-reach, reach, 5)
    Xg, Yg = np.meshgrid(xs, xs)
    ax.plot_wireframe(Xg, Yg, np.zeros_like(Xg), color="lightgray", linewidth=0.3)


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_CSV
    if not os.path.isfile(csv_path):
        print(f"ERROR: CSV not found: {csv_path}", file=sys.stderr)
        sys.exit(1)

    df = pd.read_csv(csv_path)
    print(f"Loaded {len(df)} rows from {csv_path}")

    # Figure 1: joint angles
    fig1, axes1 = plt.subplots(3, 1, figsize=(10, 7), sharex=True)
    fig1.suptitle("Joint Angles: Actual vs Desired")
    plot_angles(df, axes1)
    fig1.tight_layout()

    # Figure 2: torques + errors
    fig2, axes2 = plt.subplots(3, 1, figsize=(10, 9))
    fig2.suptitle("Torques & Tracking Errors")
    plot_torques(df, axes2[0])
    axes2[0].set_title("Joint Torques")
    plot_errors(df, axes2[1:])
    fig2.tight_layout()

    # Figure 3: 3-D trajectory
    fig3 = plt.figure(figsize=(8, 7))
    ax3d = fig3.add_subplot(111, projection="3d")
    ax3d.set_title("3-DOF Arm Trajectory Tracking")
    plot_3d_trajectory(df, ax3d)
    fig3.tight_layout()

    plt.show()


if __name__ == "__main__":
    main()
