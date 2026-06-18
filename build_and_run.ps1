#Requires -Version 5.1
<#
.SYNOPSIS
    RobotLink ビルド・シミュレーション・Python ビジュアライザー起動

.PARAMETER BuildType
    Release (既定) / Debug

.PARAMETER SkipBuild
    C++ シミュレーションのビルドをスキップ

.PARAMETER SkipSim
    シミュレーション実行をスキップ (既存 CSV を再利用)

.PARAMETER SkipPyDeps
    pip install をスキップ

.PARAMETER Clean
    build/ を削除してフルリビルド

.EXAMPLE
    .\build_and_run.ps1
    .\build_and_run.ps1 -SkipBuild -SkipSim
#>
param(
    [ValidateSet("1","2","3","4","qt6","avalonia","pyqt6","matplotlib")]
    [string] $Frontend   = "1",   # 1=Qt6  2=Avalonia  3=Qt6(alias)  4=matplotlib
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
        Die "'$cmd' が見つかりません。インストールして PATH を通してください。"
    }
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 1 : C++ シミュレーションをビルド
# ════════════════════════════════════════════════════════════════════════════
if (-not $SkipBuild) {
    Require "cmake"
    $cpuN = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors

    if ($Clean -and (Test-Path "$ROOT\build")) {
        Step "build/ を削除中"
        Remove-Item -Recurse -Force "$ROOT\build"
        Ok "削除完了"
    }

    Step "C++ シミュレーションをビルド中 ($BuildType)"
    $cache = "$ROOT\build\CMakeCache.txt"
    if (Test-Path $cache) {
        $line = (Get-Content $cache | Select-String "^CMAKE_HOME_DIRECTORY").Line
        if ($line -and -not ($line -match [Regex]::Escape($ROOT.Replace('\','/')))) {
            Remove-Item $cache -Force
        }
    }
    $genArgs = if (Test-Path "$ROOT\build\CMakeCache.txt") { @() } else { @("-G", "Visual Studio 17 2022", "-A", "x64") }
    & cmake -S "$ROOT" -B "$ROOT\build" -DCMAKE_BUILD_TYPE=$BuildType @genArgs
    if ($LASTEXITCODE -ne 0) { Die "cmake configure 失敗" }
    & cmake --build "$ROOT\build" --config $BuildType --parallel $cpuN
    if ($LASTEXITCODE -ne 0) { Die "cmake build 失敗" }
    Ok "C++ シミュレーション ビルド完了"

    if (-not $SkipPyDeps) {
        Require "python"
        Step "Python 依存関係をインストール中"
        & python -m pip install -r "$ROOT\requirements.txt" --quiet
        if ($LASTEXITCODE -ne 0) { Die "pip install 失敗" }
        Ok "pip install 完了"
    }
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 2 : シミュレーション実行 (CSV 生成)
# ════════════════════════════════════════════════════════════════════════════
$simExe = @(
    "$ROOT\build\$BuildType\robot_sim.exe",
    "$ROOT\build\Release\robot_sim.exe",
    "$ROOT\build\robot_sim.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $SkipSim) {
    if (-not $simExe) { Die "robot_sim.exe が見つかりません。先にビルドしてください。" }
    if (-not (Test-Path "$ROOT\output")) { New-Item -ItemType Directory -Force "$ROOT\output" | Out-Null }

    Step "シミュレーションを実行中 (output/sim_results.csv を生成)"
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $simExe "$ROOT\output"
    if ($LASTEXITCODE -ne 0) { Die "シミュレーション実行失敗 (終了コード $LASTEXITCODE)" }
    $sw.Stop()
    Ok "シミュレーション完了 ($($sw.Elapsed.ToString('mm\:ss\.ff')))"
} else {
    Ok "シミュレーションをスキップ (既存 CSV を使用)"
}

$csvPath = "$ROOT\output\sim_results.csv"
if (-not (Test-Path $csvPath)) {
    Warn "output/sim_results.csv が見つかりません。ビジュアライザーはデータなしで起動します。"
}

# ════════════════════════════════════════════════════════════════════════════
# STEP 3 : ビジュアライザー起動
# ════════════════════════════════════════════════════════════════════════════
switch ($Frontend) {

    { $_ -in "1","3","qt6","pyqt6" } {
        Step "Qt6 ビジュアライザーを起動します"
        $qtExe = "$ROOT\frontend_qt\build\Release\robotlink_viz_qt.exe"
        if (-not (Test-Path $qtExe)) { Die "Qt6 実行ファイルが見つかりません: $qtExe`n先に frontend_qt\build\ で cmake --build してください。" }
        Start-Process $qtExe -ArgumentList "`"$csvPath`""
        Ok "Qt6 起動: $qtExe"
    }

    { $_ -in "2","avalonia" } {
        Step "Avalonia ビジュアライザーを起動します"
        $avExe = "$ROOT\frontend_avalonia\RobotLinkAvalonia\bin\Release\net8.0\RobotLinkAvalonia.exe"
        if (-not (Test-Path $avExe)) { Die "Avalonia 実行ファイルが見つかりません: $avExe`n先に dotnet build -c Release してください。" }
        Start-Process $avExe -ArgumentList "`"$csvPath`""
        Ok "Avalonia 起動: $avExe"
    }

    { $_ -in "4","matplotlib" } {
        Step "matplotlib ビジュアライザーを起動します"
        Require "python"
        $vizScript = "$ROOT\python\visualizer_matplotlib.py"
        if (-not (Test-Path $vizScript)) { Die "python/visualizer_matplotlib.py が見つかりません" }
        & python "$vizScript" "$csvPath"
        Ok "matplotlib 終了"
    }

    default {
        Die "不明なフロントエンド: $_"
    }
}

Write-Host ""
Write-Host "使い方:" -ForegroundColor DarkCyan
Write-Host "  .\build_and_run.ps1 -Frontend 1             # Qt6 C++ ビジュアライザー (既定)"
Write-Host "  .\build_and_run.ps1 -Frontend 3             # Qt6 C++ ビジュアライザー (1 と同じ)"
Write-Host "  .\build_and_run.ps1 -Frontend 2             # Avalonia C# ビジュアライザー"
Write-Host "  .\build_and_run.ps1 -Frontend 4             # matplotlib (通常表示)"
Write-Host "  .\build_and_run.ps1 -SkipBuild -SkipSim     # 既存 CSV でビジュアライザーのみ起動"
Write-Host "  .\build_and_run.ps1 -Clean                  # フルリビルド"
Write-Host ""
