# CLAUDE.md — PID Interactive Demo

## プロジェクト概要

C++ コアライブラリ (`pid_core.dll/.so`) を Qt6・Avalonia・PySide6 の 3 フロントエンドで共有する PID シミュレーター。

```
pid/
├── core/                       # C++ ライブラリ (C ABI)
├── frontend_qt/                # Qt6 フロントエンド
├── frontend_avalonia/          # Avalonia (C#/.NET 8) フロントエンド
├── frontend_python/            # PySide6 フロントエンド
└── build_and_run.ps1           # Windows 統合ビルド・起動スクリプト
```

ビルド・起動は常に `build_and_run.ps1` 経由で行う:

```powershell
.\build_and_run.ps1 1   # Qt6
.\build_and_run.ps1 2   # Avalonia
.\build_and_run.ps1 3   # Python
```

---

## 【重要】Windows × Qt6 × vcpkg — DLL 配置の既知の落とし穴

### 問題の本質

`windeployqt` は Qt 本体の DLL を配置するが、**Qt が依存する vcpkg 製サードパーティ DLL を配置しない**。
これにより実行時に `0xC0000135 (STATUS_DLL_NOT_FOUND)` でクラッシュする。
アプリウィンドウが一切表示されずサイレントに終了するため、ビルド成功後でも「動かない」状態になる。

### 現在の対処

`build_and_run.ps1` 内の `$qtThirdPartyDlls` リストで明示的に補完コピーしている。
Qt モジュールを追加・変更した際は、このリストも更新が必要になる。

### 確定している必要 DLL（vcpkg ビルド時）

| DLL | 必要とするモジュール |
|-----|----------------------|
| `double-conversion.dll` | Qt6Core |
| `pcre2-16.dll` | Qt6Core |
| `z.dll` | Qt6Core / Qt6Gui (zlib) |
| `zstd.dll` | Qt6Core |
| `harfbuzz.dll` | Qt6Gui |
| `freetype.dll` | Qt6Gui |
| `libpng16.dll` | Qt6Gui |
| `bz2.dll` | Qt6Gui |
| `md4c.dll` | Qt6Gui (Markdown) |
| `brotlidec.dll` | freetype (Brotli フォント) |
| `brotlicommon.dll` | brotlidec |
| `jpeg62.dll` | qjpeg プラグイン |
| `libcrypto-3-x64.dll` | Qt6Network |

### 診断手順

Qt6 アプリが起動しない・すぐ終了する場合は必ずこの順で確認すること:

**Step 1 — 終了コードを確認する**
```powershell
$p = Start-Process ".\pid_qt.exe" -WorkingDirectory "." -PassThru -Wait
"ExitCode: 0x$('{0:X8}' -f [int]$p.ExitCode)"
# 0xC0000135 → DLL 不足確定
```

**Step 2 — 欠落 DLL を検出する**
```powershell
$releaseDir = ".\frontend_qt\build\Release"
$vcpkgBin   = "C:\vcpkg\installed\x64-windows\bin"
$systemPrefixes = @("api-ms-win-","ext-ms-","ntdll","kernel32","advapi32",
    "user32","gdi32","ole32","oleaut32","shell32","ws2_32","winmm",
    "version","mpr","netapi32","userenv","authz","d3d","dxgi","dwmapi",
    "imm32","uxtheme","msvcp140","vcruntime140","ucrtbase")

$knownDlls = (Get-ChildItem $releaseDir -Filter "*.dll" -Recurse).Name

foreach ($file in (Get-ChildItem $releaseDir -Include "*.dll","*.exe" -Recurse)) {
    $text = [System.Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($file.FullName))
    [regex]::Matches($text, '[A-Za-z_][A-Za-z0-9_\-]+\.dll') |
        ForEach-Object { $_.Value } | Sort-Object -Unique |
        Where-Object {
            $n = $_.ToLower()
            -not ($systemPrefixes | Where-Object { $n.StartsWith($_) }) -and
            -not ($knownDlls | Where-Object { $_.ToLower() -eq $n }) -and
            -not (Test-Path "C:\Windows\System32\$_")
        } |
        ForEach-Object { Write-Host "MISSING: $_ (in $($file.Name))" -ForegroundColor Red }
}
```

**Step 3 — 欠落 DLL を vcpkg からコピーする**
```powershell
# 検出した DLL 名を列挙してコピー
@("不足dll.dll", ...) | ForEach-Object {
    $src = "$vcpkgBin\$_"
    if (Test-Path $src) { Copy-Item $src $releaseDir -Force; Write-Host "Copied: $_" }
    else { Write-Host "NOT IN VCPKG: $_" -ForegroundColor Red }
}
```

**Step 4 — build_and_run.ps1 の `$qtThirdPartyDlls` リストに追記する**

新たに必要と判明した DLL は必ず `build_and_run.ps1` のリストに追加し、
次回ビルド時から自動コピーされるようにする。

### Qt モジュールを追加したとき

`CMakeLists.txt` に新しい Qt6 モジュール（例: `Qt6::Sql`, `Qt6::Charts`）を追加した場合、
その DLL がさらに依存する vcpkg DLL が増える可能性がある。
**ビルド後・初回起動前に必ず Step 1〜4 を実施すること。**

---

## Qt6 パス（自動検出優先順）

```
C:\vcpkg\installed\x64-windows
C:\Qt\6.9.0\msvc2022_64
C:\Qt\6.8.0\msvc2022_64
C:\Qt\6.7.0\msvc2022_64
```

`-Qt6Path` 引数で上書き可能。

---

## ビルド環境

- Windows 11、Visual Studio 2022、CMake 3.16+
- Qt6 は vcpkg 経由（`C:\vcpkg\installed\x64-windows`）
- C# は .NET 8 SDK
- Python は 3.10+（PySide6 + matplotlib + numpy）
