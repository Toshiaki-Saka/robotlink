#Requires -Version 5.1
# build_and_run.ps1 — RobotLink (3-DOF robot arm)
# Builds and runs the C++ simulation core, then launches exactly ONE of the three
# frontends. Same feel as the build_and_run.ps1 in path-planning-classics.
#
# Flow (3 steps):
#   STEP 1  Build the C++ simulation (robot_sim) with CMake
#           (first build only: derives the dynamics in Python and generates the header, 1-3 min)
#   STEP 2  Run robot_sim to produce output/sim_results.csv
#   STEP 3  Launch the chosen frontend (reads and plots the CSV the core wrote)
#
# Frontend argument … 1 = Qt6      (C++)      frontend_qt
#                     2 = Avalonia (C#)       frontend_avalonia
#                     3 = PyQt6    (Python)   frontend_python\visualizer_pyqt6.py
#   NOTE: Qt6 / Avalonia must be built beforehand (this script only launches them):
#       Qt6      : run  cmake --build  in  frontend_qt\build\
#       Avalonia : run  dotnet build -c Release  in  frontend_avalonia\RobotLinkAvalonia\
#     PyQt6 needs no prior build — the Python script is launched directly (pip: PyQt6).
#
# Usage:
#   .\build_and_run.ps1                       # no args -> default (Qt6), full run
#   .\build_and_run.ps1 1                     # launch with Qt6
#   .\build_and_run.ps1 2                     # launch with Avalonia
#   .\build_and_run.ps1 3                     # launch with PyQt6
#   .\build_and_run.ps1 3 -SkipBuild -SkipSim # launch PyQt6 only, using existing CSV
#   .\build_and_run.ps1 1 -BuildType Debug    # Qt6 / Debug build
#   .\build_and_run.ps1 -Clean                # remove build/ and do a full rebuild
param(
    # 1=Qt6 (C++) / 2=Avalonia (C#) / 3=PyQt6 (Python)
    [ValidateSet("1","2","3","qt6","avalonia","pyqt6")]
    [string] $Frontend   = "1",
    [string] $BuildType  = "Release",
    [switch] $SkipBuild,
    [switch] $SkipSim,
    [switch] $SkipPyDeps,
    [switch] $Clean
)

$ErrorActionPreference = "Stop"
$ROOT = $PSScriptRoot

function Step($msg) { Write-Host "`n==> $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "    OK  : $msg" -ForegroundColor Green }
function Warn($msg) { Write-Host "    WARN: $msg" -ForegroundColor Yellow }
function Die($msg)  { Write-Host "`nERROR: $msg" -ForegroundColor Red; exit 1 }
function Require($cmd) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) {
        Die "'$cmd' not found. Please install it and make sure it is on your PATH."
    }
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 1 : Build the C++ simulation
# ════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuild) {
    Require "cmake"
    $cpuN = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

    if ($Clean -and (Test-Path "$ROOT\build")) {
        Step "Removing build/"
        Remove-Item -Recurse -Force "$ROOT\build"
        Ok "Removed"
    }

    Step "Building the C++ simulation ($BuildType)"
    $cache = "$ROOT\build\CMakeCache.txt"
    if (Test-Path $cache) {
        $line = (Get-Content $cache | Select-String "^CMAKE_HOME_DIRECTORY").Line
        if ($line -and -not ($line -match [Regex]::Escape($ROOT.Replace('\','/')))) {
            Remove-Item $cache -Force
        }
    }
    $genArgs = if (Test-Path "$ROOT\build\CMakeCache.txt") { @() } else { @("-G", "Visual Studio 17 2022", "-A", "x64") }
    & cmake -S "$ROOT" -B "$ROOT\build" -DCMAKE_BUILD_TYPE=$BuildType @genArgs
    if ($LASTEXITCODE -ne 0) { Die "cmake configure failed" }
    & cmake --build "$ROOT\build" --config $BuildType --parallel $cpuN
    if ($LASTEXITCODE -ne 0) { Die "cmake build failed" }
    Ok "C++ simulation build complete"

    if (-not $SkipPyDeps) {
        Require "python"
        Step "Installing Python dependencies"
        & python -m pip install -r "$ROOT\requirements.txt" --quiet
        if ($LASTEXITCODE -ne 0) { Die "pip install failed" }
        Ok "pip install complete"
    }
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 2 : Run the simulation (produce the CSV)
# ════════════════════════════════════════════════════════════════════════════
$simExe = @(
    "$ROOT\build\$BuildType\robot_sim.exe",
    "$ROOT\build\Release\robot_sim.exe",
    "$ROOT\build\robot_sim.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $SkipSim) {
    if (-not $simExe) { Die "robot_sim.exe not found. Please build it first." }
    if (-not (Test-Path "$ROOT\output")) { New-Item -ItemType Directory -Force "$ROOT\output" | Out-Null }

    Step "Running the simulation (produces output/sim_results.csv)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $simExe "$ROOT\output"
    if ($LASTEXITCODE -ne 0) { Die "Simulation run failed (exit code $LASTEXITCODE)" }
    $sw.Stop()
    Ok "Simulation complete ($($sw.Elapsed.ToString('mm\:ss\.ff')))"
} else {
    Ok "Skipping simulation (using existing CSV)"
}

$csvPath = "$ROOT\output\sim_results.csv"
if (-not (Test-Path $csvPath)) {
    Warn "output/sim_results.csv not found. The visualizer will start with no data."
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 3 : Launch the visualizer
# ════════════════════════════════════════════════════════════════════════════
switch ($Frontend) {

    { $_ -in "1","qt6" } {
        Step "Launching the Qt6 visualizer"
        $qtExe = "$ROOT\frontend_qt\build\Release\robotlink_viz_qt.exe"
        if (-not (Test-Path $qtExe)) { Die "Qt6 executable not found: $qtExe`nBuild it first with cmake --build in frontend_qt\build\." }
        Start-Process $qtExe -ArgumentList "`"$csvPath`""
        Ok "Qt6 launched: $qtExe"
    }

    { $_ -in "2","avalonia" } {
        Step "Launching the Avalonia visualizer"
        $avExe = "$ROOT\frontend_avalonia\RobotLinkAvalonia\bin\Release\net8.0\RobotLinkAvalonia.exe"
        if (-not (Test-Path $avExe)) { Die "Avalonia executable not found: $avExe`nBuild it first with dotnet build -c Release." }
        Start-Process $avExe -ArgumentList "`"$csvPath`""
        Ok "Avalonia launched: $avExe"
    }

    { $_ -in "3","pyqt6" } {
        Step "Launching the PyQt6 visualizer"
        Require "python"
        $vizScript = "$ROOT\frontend_python\visualizer_pyqt6.py"
        if (-not (Test-Path $vizScript)) { Die "frontend_python/visualizer_pyqt6.py not found" }
        & python "$vizScript" "$csvPath"
        Ok "PyQt6 exited"
    }

    default {
        Die "Unknown frontend: $_"
    }
}

Write-Host ""
Write-Host "Usage:" -ForegroundColor DarkCyan
Write-Host "  .\build_and_run.ps1 1                       # Qt6 C++ visualizer (default)"
Write-Host "  .\build_and_run.ps1 2                       # Avalonia C# visualizer"
Write-Host "  .\build_and_run.ps1 3                       # PyQt6 (Python)"
Write-Host "  .\build_and_run.ps1 3 -SkipBuild -SkipSim   # launch the visualizer only, using existing CSV"
Write-Host "  .\build_and_run.ps1 -Clean                  # full rebuild"
Write-Host ""
