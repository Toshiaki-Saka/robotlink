"""
app_matplotlib.py — drop-in replacement for two_link_manipulator_workspace.py.

Same Slider + Reset/Save UI as the reference, but FK and workspace
calculations run in C++ via the tlm_core shared library.
"""

from __future__ import annotations

from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.widgets import Slider, Button

import tlm_core as tc


SCRIPT_DIR = Path(__file__).resolve().parent
OUTPUT_DIR = SCRIPT_DIR / "output"


def plot_workspace(ax, cfg: tc.TlmRobotConfig):
    """Draw the -θ2 (red) and +θ2 (green) boundary curves."""
    # -θ2 side: θ2 ∈ [θ2_min, 0]
    a = tc.compute_workspace(cfg, cfg.theta2_min, 0.0, 100)
    ax.plot(a.a_x, a.a_y, "r", label="Workspace boundary (-theta2)")
    ax.plot(a.b_x, a.b_y, "r")
    ax.plot(a.c_x, a.c_y, "r")
    ax.plot(a.d_x, a.d_y, "r")
    # +θ2 side: θ2 ∈ [0, θ2_max]
    b = tc.compute_workspace(cfg, 0.0, cfg.theta2_max, 100)
    ax.plot(b.a_x, b.a_y, "g", label="Workspace boundary (+theta2)")
    ax.plot(b.b_x, b.b_y, "g")
    ax.plot(b.c_x, b.c_y, "g")
    ax.plot(b.d_x, b.d_y, "g")


def main() -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    cfg = tc.TlmRobotConfig.default()

    theta1_init = 0.0
    theta2_init = 0.0

    fig, ax = plt.subplots(figsize=(8, 8))
    plt.subplots_adjust(left=0.15, bottom=0.28)

    plot_workspace(ax, cfg)

    pose = tc.forward_kinematics(cfg, theta1_init, theta2_init)
    line, = ax.plot(
        [pose.base[0], pose.joint2[0], pose.end[0]],
        [pose.base[1], pose.joint2[1], pose.end[1]],
        "o-", lw=3, label="Manipulator")
    end_effector, = ax.plot(pose.end[0], pose.end[1], "bo", label="End Effector")

    width = (cfg.l1 + cfg.l2) * 1.1
    ax.set_xlim(-width, width); ax.set_ylim(-width, width)
    ax.set_aspect("equal")
    ax.set_xlabel("X"); ax.set_ylabel("Y")
    ax.set_title("Two-Link Manipulator and Workspace (C++ core)")
    ax.grid(True); ax.legend(loc="upper right")

    info_text = ax.text(
        0.02, 0.98, "",
        transform=ax.transAxes, verticalalignment="top")

    ax_theta1 = plt.axes((0.20, 0.14, 0.65, 0.03), facecolor="lightgoldenrodyellow")
    ax_theta2 = plt.axes((0.20, 0.09, 0.65, 0.03), facecolor="lightgoldenrodyellow")
    slider_theta1 = Slider(ax_theta1, "Theta1 [rad]",
                            cfg.theta1_min, cfg.theta1_max, valinit=theta1_init)
    slider_theta2 = Slider(ax_theta2, "Theta2 [rad]",
                            cfg.theta2_min, cfg.theta2_max, valinit=theta2_init)

    ax_reset = plt.axes((0.02, 0.09, 0.10, 0.05))
    ax_save  = plt.axes((0.02, 0.16, 0.10, 0.05))
    button_reset = Button(ax_reset, "Reset")
    button_save  = Button(ax_save,  "Save")

    def update(_):
        t1 = slider_theta1.val; t2 = slider_theta2.val
        pose = tc.forward_kinematics(cfg, t1, t2)
        line.set_xdata([pose.base[0], pose.joint2[0], pose.end[0]])
        line.set_ydata([pose.base[1], pose.joint2[1], pose.end[1]])
        end_effector.set_xdata([pose.end[0]])
        end_effector.set_ydata([pose.end[1]])
        info_text.set_text(
            f"theta1 = {t1:.3f} rad\n"
            f"theta2 = {t2:.3f} rad\n"
            f"x_end  = {pose.end[0]:.3f}\n"
            f"y_end  = {pose.end[1]:.3f}"
        )
        fig.canvas.draw_idle()

    def reset(_):
        slider_theta1.reset(); slider_theta2.reset()

    def save(_):
        save_path = OUTPUT_DIR / "two_link_workspace.png"
        fig.savefig(save_path, dpi=150, bbox_inches="tight")
        print(f"Saved: {save_path}")

    slider_theta1.on_changed(update)
    slider_theta2.on_changed(update)
    button_reset.on_clicked(reset)
    button_save.on_clicked(save)

    update(None)
    plt.show()


if __name__ == "__main__":
    main()
