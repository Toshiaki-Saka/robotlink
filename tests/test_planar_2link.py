"""
test_examples.py — cross-example smoke test for multi-gui-control-templates.

Runs every control example through its adapter and asserts each produces a
non-trivial time-series. Examples whose shared library is not built are skipped
rather than failing.

    ctest --test-dir build
    python tests/test_examples.py
"""
from __future__ import annotations

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "gui", "python"))

from adapters import run_all  # noqa: E402


def main() -> int:
    results = run_all()
    failures = 0
    ran = 0
    for r in results:
        if r.description.startswith("error:") and "Could not locate" in r.description:
            print(f"SKIP  {r.name}: library not built")
            continue
        ran += 1
        if r.description.startswith("error:"):
            print(f"FAIL  {r.name}: {r.description}")
            failures += 1
            continue
        n_traces = sum(len(p.traces) for p in r.plots)
        n_points = sum(len(t.y) for p in r.plots for t in p.traces)
        if n_traces == 0 or n_points < 2:
            print(f"FAIL  {r.name}: empty result ({n_traces} traces, {n_points} pts)")
            failures += 1
            continue
        print(f"OK    {r.name}: {len(r.plots)} plot(s), {n_traces} trace(s), "
              f"{n_points} pts")

    if ran == 0:
        print("ERROR: no example libraries were available to test.")
        return 1
    print(f"\n{ran} example(s) exercised, {failures} failure(s).")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
