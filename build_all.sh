#!/usr/bin/env bash
# build_all.sh — build the C++ core, run the smoke test, and build the
# Qt6 frontend. For Windows use build_all.bat.
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"

echo "==> Building C++ core"
mkdir -p "$ROOT/core/build"
cd "$ROOT/core/build"
cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null
cmake --build . -j

echo "==> Running C++ smoke test"
cmake --build . --target tlm_core_smoke -j > /dev/null
./tlm_core_smoke

echo "==> Building Qt6 frontend"
if command -v qmake6 >/dev/null 2>&1 || command -v qmake >/dev/null 2>&1; then
    mkdir -p "$ROOT/frontend_qt/build"
    cd "$ROOT/frontend_qt/build"
    cmake .. -DCMAKE_BUILD_TYPE=Release > /dev/null
    cmake --build . -j
    echo "    -> $ROOT/frontend_qt/build/tlm_qt"
else
    echo "    (skipped — Qt 6 not detected)"
fi

echo
echo "==> Python frontend: ready (build the core first as above)."
echo "    To install Python deps:"
echo "      pip install -r $ROOT/frontend_python/requirements.txt"
echo
echo "==> Avalonia frontend: build separately with:"
echo "      cd $ROOT/frontend_avalonia/TlmAvalonia && dotnet run -c Release"
