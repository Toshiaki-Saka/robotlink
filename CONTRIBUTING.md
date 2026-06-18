# Contributing to RobotLink

Thanks for your interest in improving RobotLink. This document explains how to
set up the project, the conventions to follow, and how to submit changes.

## Getting started

1. Fork the repository and clone your fork.
2. Install build dependencies:
   ```bash
   pip install sympy
   pip install numpy pandas matplotlib   # for the visualizer only
   ```
3. Build and run once to confirm your environment works:
   ```bash
   cmake -B build -S .
   cmake --build build
   ./build/robot_sim
   ```

## Project conventions

- **C++**: C++20, header-only where practical. Keep the public-facing physics in
  `include/`; `src/` is for the executable entry point only.
- **Generated code**: never edit `generated/dynamics_generated.hpp` by hand — it
  is overwritten on every build. Change `python/derive_and_export.py` instead.
- **Formatting**: 4-space indentation, no tabs. Keep lines under ~100 columns.
- **Naming**: `snake_case` for functions and variables, `PascalCase` for types,
  `robot::` namespace for the simulation code.

## Making a change

1. Create a topic branch: `git checkout -b feature/short-description`.
2. Make your change with a clear, focused commit history.
3. If you change physics or numerics, add or update a test (see below).
4. Make sure the project builds cleanly on your platform with no new warnings.
5. Push and open a pull request against `main`.

## Pull request checklist

- [ ] The project configures and builds (`cmake -B build -S . && cmake --build build`).
- [ ] `robot_sim` runs and produces `output/sim_results.csv`.
- [ ] No build artifacts (`build/`, `generated/`, `output/`, `*.exe`) are committed.
- [ ] New behavior is covered by a test where feasible.
- [ ] Commit messages are descriptive.

## Reporting bugs

Open an issue and include:

- Your OS, compiler version, CMake version, and Python/SymPy version.
- The exact commands you ran.
- The full error output (configure, build, or runtime).

## Suggesting features

The roadmap in the README lists planned directions. If your idea is large
(e.g. generalizing the dynamics derivation), open an issue to discuss the design
before writing code — it saves everyone time.

## Code of Conduct

Be respectful and constructive. Assume good faith. Harassment of any kind is not
tolerated.

## License

By contributing, you agree that your contributions will be licensed under the
Apache License 2.0, the same license that covers the project.
