#Requires -Version 5.1
<#
.SYNOPSIS
    One-shot script that builds, tests, and runs the RobotLink simulation.

.PARAMETER SkipTests
    Skip the CTest test run.

.PARAMETER SkipVisualize
    Skip launching the Python visualizer.

.PARAMETER Clean
    Remove build/ and do a full rebuild.

.PARAMETER UseSystemEigen
    Use a system-installed Eigen instead of FetchContent.

.EXAMPLE
    .\run_sim.ps1
    .\run_sim.ps1 -Clean -SkipVisualize
    .\run_sim.ps1 -SkipTests
#>

param(
    [switch]$SkipTests,
    [switch]$SkipVisualize,
    [switch]$Clean,
    [switch]$UseSystemEigen
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Helper functions ──────────────────────────────────────────────────────────

function Write-Header([string]$msg) {
    Write-Host ""
    Write-Host "=" * 60 -ForegroundColor Cyan
    Write-Host "  $msg" -ForegroundColor Cyan
    Write-Host "=" * 60 -ForegroundColor Cyan
}

function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host ">>> $msg" -ForegroundColor Yellow
}

function Write-OK([string]$msg) {
    Write-Host "[OK] $msg" -ForegroundColor Green
}

function Write-Fail([string]$msg) {
    Write-Host "[FAIL] $msg" -ForegroundColor Red
}

function Invoke-Step([string]$label, [scriptblock]$block) {
    Write-Step $label
    & $block
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "$label failed (exit code $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
    Write-OK "$label complete"
}

# ── Project root ──────────────────────────────────────────────────────────────

$Root     = $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$OutDir   = Join-Path $Root "output"

Set-Location $Root

Write-Header "RobotLink build & simulation"
Write-Host "Project : $Root"
Write-Host "Date    : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

# ── Check for required tools ──────────────────────────────────────────────────

Write-Step "Checking for required tools..."

foreach ($tool in @("cmake", "python")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Fail "'$tool' not found on PATH. Please install it and re-run."
        exit 1
    }
    $ver = & $tool --version 2>&1 | Select-Object -First 1
    Write-OK "$tool : $ver"
}

# Check CMake version (3.20+)
$cmakeVerStr = (& cmake --version 2>&1 | Select-Object -First 1) -replace "cmake version ", ""
$cmakeVer    = [version]$cmakeVerStr
if ($cmakeVer -lt [version]"3.20") {
    Write-Fail "CMake 3.20 or newer is required (found: $cmakeVerStr)"
    exit 1
}

# ── Check and install Python packages ─────────────────────────────────────────

Write-Step "Checking Python packages..."

$reqFile = Join-Path $Root "requirements.txt"
$missing = @()
foreach ($pkg in @("sympy", "numpy", "pandas", "matplotlib")) {
    $check = & python -c "import $pkg" 2>&1
    if ($LASTEXITCODE -ne 0) { $missing += $pkg }
    else { Write-OK "$pkg import OK" }
}

if ($missing.Count -gt 0) {
    Write-Step "Installing missing packages: $($missing -join ', ')"
    & python -m pip install --quiet -r $reqFile
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "pip install failed"
        exit 1
    }
    Write-OK "Package installation complete"
}

# ── Clean build ───────────────────────────────────────────────────────────────

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "Removing build/..."
    Remove-Item -Recurse -Force $BuildDir
    Write-OK "build/ removed"
}

# ── CMake configure ───────────────────────────────────────────────────────────

$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $Root,
    "-DCMAKE_BUILD_TYPE=Release"
)
if ($UseSystemEigen)  { $cmakeArgs += "-DROBOTLINK_USE_SYSTEM_EIGEN=ON" }
if ($SkipTests)       { $cmakeArgs += "-DROBOTLINK_BUILD_TESTS=OFF" }

Invoke-Step "CMake configure" {
    & cmake @cmakeArgs
}

# ── Build ─────────────────────────────────────────────────────────────────────

$cpuCount = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
Invoke-Step "CMake build (parallelism: $cpuCount)  (first build takes a few minutes for the SymPy derivation)" {
    & cmake --build $BuildDir --config Release --parallel $cpuCount
}

# ── Test ──────────────────────────────────────────────────────────────────────

if (-not $SkipTests) {
    Invoke-Step "Run tests with CTest" {
        & ctest --test-dir $BuildDir --build-config Release --output-on-failure
    }
}

# ── Locate the executable ─────────────────────────────────────────────────────

Write-Step "Locating the simulation executable..."

$candidates = @(
    (Join-Path $BuildDir "Release\robot_sim.exe"),
    (Join-Path $BuildDir "robot_sim.exe"),
    (Join-Path $BuildDir "robot_sim")
)
$simExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $simExe) {
    Write-Fail "robot_sim executable not found. Please check the build."
    exit 1
}
Write-OK "Executable: $simExe"

# ── Run the simulation ────────────────────────────────────────────────────────

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force $OutDir | Out-Null }

$csvPath = Join-Path $OutDir "sim_results.csv"
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

Invoke-Step "Run the simulation" {
    & $simExe $OutDir
}

$stopwatch.Stop()
$elapsed = $stopwatch.Elapsed.ToString("mm\:ss\.ff")

Write-Host ""
Write-Host "Simulation complete (elapsed: $elapsed)" -ForegroundColor Green
Write-Host "Result CSV: $csvPath"

if (Test-Path $csvPath) {
    $lines = (Get-Content $csvPath | Measure-Object -Line).Lines
    Write-Host "CSV rows (including header): $lines"
}

# ── Launch the visualizer ─────────────────────────────────────────────────────

if (-not $SkipVisualize) {
    $vizScript = Join-Path $Root "frontend_python\visualizer.py"
    if (Test-Path $vizScript) {
        Write-Step "Launching the visualizer..."
        Write-Host "  (closing the window ends the script)" -ForegroundColor DarkGray
        & python $vizScript
    } else {
        Write-Host "Visualizer (frontend_python/visualizer.py) not found. Skipping."
    }
}

# ── Done ──────────────────────────────────────────────────────────────────────

Write-Header "All steps complete"
Write-Host "Output directory : $OutDir"
Write-Host ""
Write-Host "Re-run options:"
Write-Host "  .\run_sim.ps1                   # normal build & run"
Write-Host "  .\run_sim.ps1 -Clean            # clean build"
Write-Host "  .\run_sim.ps1 -SkipTests        # skip tests"
Write-Host "  .\run_sim.ps1 -SkipVisualize    # skip the visualizer"
