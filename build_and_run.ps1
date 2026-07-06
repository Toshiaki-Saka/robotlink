#Requires -Version 5.1
# build_and_run.ps1 — RobotLink (3-DOF ロボットアーム)
# C++ シミュレーションコアをビルド＆実行し、3つのフロントエンドから「1つだけ」を
# 選んで起動する。path-planning-classics の build_and_run.ps1 と同じ操作感。
#
# 実行フロー (3 ステップ):
#   STEP 1  C++ シミュレーション (robot_sim) を CMake でビルド
#           (初回のみ Python で動力学を導出しヘッダ生成。1〜3 分)
#   STEP 2  robot_sim を実行し output/sim_results.csv を生成
#   STEP 3  選んだフロントエンドを起動 (コアが書き出した CSV を読み込んで描画)
#
# 引数 Frontend … 1 = Qt6      (C++)      frontend_qt
#                 2 = Avalonia (C#)       frontend_avalonia
#                 3 = PyQt6    (Python)   frontend_python\visualizer_pyqt6.py
#   ※ Qt6 / Avalonia は事前にビルドしておくこと (本スクリプトは起動のみ):
#       Qt6      : frontend_qt\build\ で  cmake --build
#       Avalonia : frontend_avalonia\RobotLinkAvalonia\ で  dotnet build -c Release
#     PyQt6 は Python スクリプトを直接起動するため事前ビルド不要 (pip: PyQt6)。
#
# 使い方:
#   .\build_and_run.ps1                       # 引数なし → 既定 (Qt6) で通し実行
#   .\build_and_run.ps1 1                     # Qt6 で起動
#   .\build_and_run.ps1 2                     # Avalonia で起動
#   .\build_and_run.ps1 3                     # PyQt6 で起動
#   .\build_and_run.ps1 3 -SkipBuild -SkipSim # 既存 CSV で PyQt6 のみ起動
#   .\build_and_run.ps1 1 -BuildType Debug    # Qt6 / Debug ビルド
#   .\build_and_run.ps1 -Clean                # build/ を消してフルリビルド
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

    { $_ -in "1","qt6" } {
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

    { $_ -in "3","pyqt6" } {
        Step "PyQt6 ビジュアライザーを起動します"
        Require "python"
        $vizScript = "$ROOT\frontend_python\visualizer_pyqt6.py"
        if (-not (Test-Path $vizScript)) { Die "frontend_python/visualizer_pyqt6.py が見つかりません" }
        & python "$vizScript" "$csvPath"
        Ok "PyQt6 終了"
    }

    default {
        Die "不明なフロントエンド: $_"
    }
}

Write-Host ""
Write-Host "使い方:" -ForegroundColor DarkCyan
Write-Host "  .\build_and_run.ps1 1                       # Qt6 C++ ビジュアライザー (既定)"
Write-Host "  .\build_and_run.ps1 2                       # Avalonia C# ビジュアライザー"
Write-Host "  .\build_and_run.ps1 3                       # PyQt6 (Python)"
Write-Host "  .\build_and_run.ps1 3 -SkipBuild -SkipSim   # 既存 CSV でビジュアライザーのみ起動"
Write-Host "  .\build_and_run.ps1 -Clean                  # フルリビルド"
Write-Host ""
