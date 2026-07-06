#Requires -Version 5.1
<#
.SYNOPSIS
    RobotLink のビルド・テスト・シミュレーション実行を一括で行うスクリプト

.PARAMETER SkipTests
    CTest によるテストをスキップする

.PARAMETER SkipVisualize
    Python ビジュアライザーの起動をスキップする

.PARAMETER Clean
    build/ を削除してフルリビルドする

.PARAMETER UseSystemEigen
    FetchContent を使わず、システムインストール済みの Eigen を使う

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

# ── ヘルパー関数 ──────────────────────────────────────────────────────────────

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
        Write-Fail "$label が失敗しました (終了コード $LASTEXITCODE)"
        exit $LASTEXITCODE
    }
    Write-OK "$label 完了"
}

# ── プロジェクトルート ────────────────────────────────────────────────────────

$Root     = $PSScriptRoot
$BuildDir = Join-Path $Root "build"
$OutDir   = Join-Path $Root "output"

Set-Location $Root

Write-Header "RobotLink ビルド & シミュレーション"
Write-Host "プロジェクト: $Root"
Write-Host "日時        : $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"

# ── 前提ツールの確認 ──────────────────────────────────────────────────────────

Write-Step "前提ツールを確認しています..."

foreach ($tool in @("cmake", "python")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        Write-Fail "'$tool' が PATH に見つかりません。インストールして再実行してください。"
        exit 1
    }
    $ver = & $tool --version 2>&1 | Select-Object -First 1
    Write-OK "$tool : $ver"
}

# CMake バージョン確認 (3.20+)
$cmakeVerStr = (& cmake --version 2>&1 | Select-Object -First 1) -replace "cmake version ", ""
$cmakeVer    = [version]$cmakeVerStr
if ($cmakeVer -lt [version]"3.20") {
    Write-Fail "CMake 3.20 以上が必要です (現在: $cmakeVerStr)"
    exit 1
}

# ── Python パッケージの確認とインストール ─────────────────────────────────────

Write-Step "Python パッケージを確認しています..."

$reqFile = Join-Path $Root "requirements.txt"
$missing = @()
foreach ($pkg in @("sympy", "numpy", "pandas", "matplotlib")) {
    $check = & python -c "import $pkg" 2>&1
    if ($LASTEXITCODE -ne 0) { $missing += $pkg }
    else { Write-OK "$pkg インポート OK" }
}

if ($missing.Count -gt 0) {
    Write-Step "不足パッケージをインストールします: $($missing -join ', ')"
    & python -m pip install --quiet -r $reqFile
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "pip install が失敗しました"
        exit 1
    }
    Write-OK "パッケージインストール完了"
}

# ── クリーンビルド ────────────────────────────────────────────────────────────

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Step "build/ を削除しています..."
    Remove-Item -Recurse -Force $BuildDir
    Write-OK "build/ 削除完了"
}

# ── CMake 設定 ────────────────────────────────────────────────────────────────

$cmakeArgs = @(
    "-B", $BuildDir,
    "-S", $Root,
    "-DCMAKE_BUILD_TYPE=Release"
)
if ($UseSystemEigen)  { $cmakeArgs += "-DROBOTLINK_USE_SYSTEM_EIGEN=ON" }
if ($SkipTests)       { $cmakeArgs += "-DROBOTLINK_BUILD_TESTS=OFF" }

Invoke-Step "CMake 設定" {
    & cmake @cmakeArgs
}

# ── ビルド ────────────────────────────────────────────────────────────────────

$cpuCount = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors
Invoke-Step "CMake ビルド (並列度: $cpuCount)  ※初回は SymPy 導出で数分かかります" {
    & cmake --build $BuildDir --config Release --parallel $cpuCount
}

# ── テスト ────────────────────────────────────────────────────────────────────

if (-not $SkipTests) {
    Invoke-Step "CTest によるテスト実行" {
        & ctest --test-dir $BuildDir --build-config Release --output-on-failure
    }
}

# ── 実行ファイルの検索 ────────────────────────────────────────────────────────

Write-Step "シミュレーション実行ファイルを検索しています..."

$candidates = @(
    (Join-Path $BuildDir "Release\robot_sim.exe"),
    (Join-Path $BuildDir "robot_sim.exe"),
    (Join-Path $BuildDir "robot_sim")
)
$simExe = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $simExe) {
    Write-Fail "robot_sim の実行ファイルが見つかりません。ビルドを確認してください。"
    exit 1
}
Write-OK "実行ファイル: $simExe"

# ── シミュレーション実行 ──────────────────────────────────────────────────────

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Force $OutDir | Out-Null }

$csvPath = Join-Path $OutDir "sim_results.csv"
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

Invoke-Step "シミュレーション実行" {
    & $simExe $OutDir
}

$stopwatch.Stop()
$elapsed = $stopwatch.Elapsed.ToString("mm\:ss\.ff")

Write-Host ""
Write-Host "シミュレーション完了 (経過時間: $elapsed)" -ForegroundColor Green
Write-Host "結果CSV: $csvPath"

if (Test-Path $csvPath) {
    $lines = (Get-Content $csvPath | Measure-Object -Line).Lines
    Write-Host "CSV 行数 (ヘッダー込み): $lines"
}

# ── ビジュアライザー起動 ──────────────────────────────────────────────────────

if (-not $SkipVisualize) {
    $vizScript = Join-Path $Root "frontend_python\visualizer.py"
    if (Test-Path $vizScript) {
        Write-Step "ビジュアライザーを起動しています..."
        Write-Host "  (ウィンドウを閉じるとスクリプトが終了します)" -ForegroundColor DarkGray
        & python $vizScript
    } else {
        Write-Host "ビジュアライザー (frontend_python/visualizer.py) が見つかりません。スキップします。"
    }
}

# ── 完了 ──────────────────────────────────────────────────────────────────────

Write-Header "全ステップ完了"
Write-Host "出力ディレクトリ : $OutDir"
Write-Host ""
Write-Host "再実行オプション:"
Write-Host "  .\run_sim.ps1                   # 通常ビルド & 実行"
Write-Host "  .\run_sim.ps1 -Clean            # クリーンビルド"
Write-Host "  .\run_sim.ps1 -SkipTests        # テストをスキップ"
Write-Host "  .\run_sim.ps1 -SkipVisualize    # ビジュアライザーをスキップ"
