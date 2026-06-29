# CLAUDE.md — tlm (Two-Link Manipulator)

## プロジェクト概要

平面2リンクマニピュレータのワークスペース可視化デモ。
C++ コアライブラリ（`tlm_core.dll`）を Qt6・Avalonia・Python の3フロントエンドで共有する。

```
tlm/
├── core/                        # C++ ライブラリ (C ABI, tlm_core.dll)
│   ├── include/tlm_core.h
│   ├── src/tlm_core.cpp
│   └── CMakeLists.txt
├── frontend_qt/                 # Qt6 / C++ フロントエンド
│   ├── main.cpp, MainWindow.{hpp,cpp}, Widgets.{hpp,cpp}
│   └── CMakeLists.txt
├── frontend_avalonia/TlmAvalonia/
│   ├── Native/{TlmCoreNative.cs, TlmSolver.cs}
│   ├── Models/{GridLineMarker.cs, BoundaryPolyline.cs}
│   ├── ViewModels/MainWindowViewModel.cs
│   ├── Views/MainWindow.{axaml,axaml.cs}
│   └── TlmAvalonia.csproj
└── frontend_python/             # PySide6 / matplotlib フロントエンド
```

---

## ビルドと実行

### コアライブラリ（全フロントエンド共通・最初に必ずビルド）

```powershell
cd core
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
# → core\build\Release\tlm_core.dll
```

### Avalonia フロントエンド

```powershell
cd frontend_avalonia\TlmAvalonia
dotnet run -c Release
```

`.csproj` が `core\build\Release\tlm_core.dll` を自動コピーする。

### Qt6 フロントエンド

```powershell
cd frontend_qt
mkdir build; cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:\Qt\6.7.0\msvc2019_64"
cmake --build . --config Release
.\Release\tlm_qt.exe
```

### Python フロントエンド

```powershell
cd frontend_python
pip install -r requirements.txt
python app_pyside6.py
```

---

## 計算の概要

```
順運動学（base_x, base_y を原点として）:
  joint2 = (base_x + l1·cosθ1,           base_y + l1·sinθ1)
  end    = (joint2.x + l2·cos(θ1+θ2),    joint2.y + l2·sin(θ1+θ2))

ワークスペース境界（曲線 4 本 × 2 回）:
  -θ2 側（赤）: t2 ∈ [θ2_min, 0]
  +θ2 側（緑）: t2 ∈ [0, θ2_max]
  各側で curve A（θ2固定,θ1掃引）/ B / C / D の 4 曲線 × 100 サンプル

デフォルト: l1=l2=1.0, θ1∈[-π/2, π/2], θ2∈[-π/2, π/2]
```

---

## 【重要】Avalonia 11 の既知の落とし穴

詳細は `docs/avalonia-notes.md` を参照。核心だけ以下に示す。

### 1. `HeaderedContentControl` は使わない
FluentTheme でスタイルが崩れる。必ず `TextBlock + Border + StackPanel` で代替。

```xml
<TextBlock Text="Link lengths" FontWeight="Bold"/>
<Border BorderBrush="Gray" BorderThickness="1" Padding="6" CornerRadius="2">
    <StackPanel Spacing="4">...</StackPanel>
</Border>
```

### 2. `Canvas` のグリッド線は `Line` 要素（`StartPoint`/`EndPoint`）で描く
`X1/Y1/X2/Y2` は Avalonia では使えない。`Grid.ShowGridLines` も Canvas では無効。

```xml
<Line StartPoint="0,100" EndPoint="580,100" Stroke="#E0E0E0" StrokeThickness="1"/>
```

### 3. `Slider` / `NumericUpDown` は `Mode=TwoWay` を必ず付ける
Avalonia のデフォルトは `OneWay`。付け忘れると ViewModel に値が届かない。

```xml
<Slider Value="{Binding Theta1, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding L1, Mode=TwoWay}"/>
```

### 4. `Polyline.Points` には `AvaloniaList<Point>` を使う
`List<Point>` や配列ではコレクション変更通知が届かず UI が更新されない。

```csharp
public AvaloniaList<Point> BoundaryPoints { get; } = new();
BoundaryPoints.Clear();
BoundaryPoints.Add(new Point(...));
```

### 5. カスタムコントロールは `StyledProperty` + `AffectsRender`
通常の CLR プロパティではバインディングも再描画トリガーも機能しない。
`BoundsProperty` を `AffectsRender` に含めないとリサイズで再描画されない。

```csharp
static WorkspaceOverlay()
{
    AffectsRender<WorkspaceOverlay>(SegmentsProperty, BoundsProperty);
}
```

### 6. `Render()` 内のポリラインは `StreamGeometry` で描く
`Control.Render(DrawingContext)` オーバーライド内では Polyline コントロールは使えない。

```csharp
var geo = new StreamGeometry();
using (var gc = geo.Open())
{
    gc.BeginFigure(pts[0], false);
    foreach (var p in pts.Skip(1)) gc.LineTo(p);
    gc.EndFigure(false);
}
ctx.DrawGeometry(null, new Pen(Brushes.Red, 1.5), geo);
```

### 7. `AvaloniaXamlLoader.Load(this)` を忘れずに呼ぶ
Source Generator を使わない場合、コンストラクタで必ず呼ぶ。ないと AXAML が反映されない。

### 8. `FormattedText` は `CultureInfo.InvariantCulture` を渡す
ロケールによって小数点がカンマになるため、数値フォーマットには常に InvariantCulture を使う。

---

## 【重要】Qt6 × vcpkg — DLL 配置の落とし穴

`windeployqt` は Qt 本体の DLL を配置するが **vcpkg 製サードパーティ DLL を配置しない**。
起動直後に `0xC0000135 (DLL_NOT_FOUND)` でサイレントにクラッシュする。

**確認手順:**
```powershell
$p = Start-Process ".\Release\tlm_qt.exe" -PassThru -Wait
"ExitCode: 0x$('{0:X8}' -f [int]$p.ExitCode)"
# 0xC0000135 → DLL 不足
```

**必要になりやすい vcpkg DLL（`C:\vcpkg\installed\x64-windows\bin\`）:**
`double-conversion.dll`, `z.dll`, `bz2.dll`, `freetype.dll`, `harfbuzz.dll`,
`libpng16.dll`, `brotlidec.dll`, `brotlicommon.dll`


---

## Polyline が表示されないときのチェックリスト

詳細は `docs/troubleshooting-signal-lines.md` を参照。

1. `tlm_core.dll` がビルド出力（`bin\Release\net8.0\`）に存在するか
2. ウィンドウ下部の StatusMessage を確認（`"Error"` があればシミュレーション失敗）
3. `ItemsControl.ItemsPanel` の Canvas 寸法が外側 Canvas と一致しているか
4. `RebuildPlot()` / `BoundaryPoints.Clear()` が UI スレッドから呼ばれているか
5. `DataContext = new MainWindowViewModel()` が `App.axaml.cs` に存在するか

---

## ファイルと役割の対応

| ファイル | 役割 |
|---------|------|
| `core/include/tlm_core.h` | C ABI（変更時は全フロントエンドに影響） |
| `frontend_avalonia/.../Native/TlmCoreNative.cs` | P/Invoke 定義 |
| `frontend_avalonia/.../Native/TlmSolver.cs` | 計算ロジック（C++ DLL を呼ぶ） |
| `frontend_avalonia/.../ViewModels/MainWindowViewModel.cs` | MVVM ViewModel |
| `frontend_avalonia/.../Views/MainWindow.axaml` | UI レイアウト |
| `docs/avalonia-notes.md` | Avalonia 落とし穴の詳細リスト |
| `docs/troubleshooting-signal-lines.md` | 描画デバッグの体系的手順 |

## 実装メモ

- Avalonia 版は Qt フロントエンドをリファレンスに、見た目・動作を合わせて実装。
- ワークスペース境界曲線（`WorkspaceOverlay`）はカスタムコントロール
  （`Control` サブクラス）として実装。Canvas サイズは `580×580` 固定。
