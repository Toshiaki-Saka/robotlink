"""
gallery_app.py — unified launcher/gallery for the 4 control examples.

Demonstrates the "1 control core × shared GUI" pattern: the same launcher drives
any example through its C ABI adapter. Pick an example and its plots are drawn;
in ``--save`` mode every example is rendered into one figure.

    python gallery_app.py                 # interactive, example dropdown
    python gallery_app.py --example pid    # show one example
    python gallery_app.py --save out.png   # render all examples (headless)
"""
from __future__ import annotations

import argparse
import sys

import matplotlib
import matplotlib.pyplot as plt

from adapters import run, run_all, EXAMPLES, RunResult


def _draw_result(fig, result: RunResult) -> None:
    fig.clear()
    n = len(result.plots) or 1
    fig.suptitle(f"{result.name} — {result.description}", fontsize=11)
    for i, p in enumerate(result.plots, 1):
        ax = fig.add_subplot(1, n, i)
        for tr in p.traces:
            ax.plot(tr.x, tr.y, tr.style, label=tr.label)
        ax.set_title(p.title, fontsize=9)
        ax.set_xlabel(p.xlabel); ax.set_ylabel(p.ylabel)
        if p.equal_aspect:
            ax.set_aspect("equal")
        if len(p.traces) > 1:
            ax.legend(fontsize=7)
    fig.tight_layout(rect=(0, 0, 1, 0.95))


def _print_metrics(result: RunResult) -> None:
    if result.metrics:
        print(f"  [{result.name}] " +
              "  ".join(f"{k}={v:.3f}" for k, v in result.metrics.items()))


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="Gallery of 4 control examples.")
    ap.add_argument("--example", choices=list(EXAMPLES), help="show one example")
    ap.add_argument("--save", metavar="PNG", help="render all examples to file")
    args = ap.parse_args(argv)

    if args.save:
        matplotlib.use("Agg")
        results = run_all()
        for r in results:
            _print_metrics(r)
        rows = len(results)
        maxcols = max((len(r.plots) or 1) for r in results)
        fig = plt.figure(figsize=(4.5 * maxcols, 3.2 * rows))
        for ri, r in enumerate(results):
            for ci, p in enumerate(r.plots):
                ax = fig.add_subplot(rows, maxcols, ri * maxcols + ci + 1)
                for tr in p.traces:
                    ax.plot(tr.x, tr.y, tr.style, label=tr.label)
                ax.set_title(f"{r.name}: {p.title}", fontsize=8)
                if p.equal_aspect:
                    ax.set_aspect("equal")
                if len(p.traces) > 1:
                    ax.legend(fontsize=6)
        fig.tight_layout()
        fig.savefig(args.save, dpi=120)
        print(f"\nsaved: {args.save}")
        return 0

    name = args.example or "pid"
    result = run(name)
    _print_metrics(result)
    fig = plt.figure(figsize=(10, 4))
    _draw_result(fig, result)
    plt.show()
    return 0


if __name__ == "__main__":
    sys.exit(main())
