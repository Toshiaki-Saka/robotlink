# 信号線（Polyline）が表示されないときのトラブルシューティング

このドキュメントは `MainWindow.axaml` の Series `ItemsControl` で Polyline が描画されない原因と対処法を体系的にまとめたものです。

---

## 症状チェックリスト

| 症状 | 最初に疑うべき原因 |
|---|---|
| グリッド線は出るが信号線だけ出ない | シミュレーション失敗 / `Enabled=false` / Points 空 |
| グリッドも信号線も何も出ない | DataContext 未設定 / Native DLL 未発見 |
| 初回は出るが Run 後に消える | `_sims` 辞書の不整合 / スレッド違反 |
| 一部のケースだけ出ない | 個別ケースのシミュレーション例外 |
| 起動直後は出るがパラメータ変更後に消える | `SyncEditorToCase` → `Run` の連鎖が途切れている |

---

## 原因 1 — Native DLL が見つからない / シミュレーション失敗

### 原因

`MsdSolver.Simulate` 内で `MsdCoreNative.Simulate` が `IntPtr.Zero` を返すか例外を投げると、
`Run()` の catch ブロックで `_sims[entry] = null` になる。
`RebuildPlot` は `null` エントリをスキップするため **全線が描画されない**。

```csharp
// MsdSolver.cs:94-96
var handle = MsdCoreNative.Simulate(ref cn, ref sn);
if (handle == IntPtr.Zero)
    throw new InvalidOperationException("msd_core_simulate failed");
```

### 確認方法

1. ウィンドウ下部の **StatusMessage** を確認する。
   - `"Simulation failed for N case(s)"` が表示されていれば DLL 側の問題。
2. `msd_core.dll`（または `libmsd_core.so`）がビルド出力ディレクトリに存在するか確認する。

```powershell
# ビルド後の出力フォルダを確認
Get-ChildItem "bin\Debug\net8.0\" -Filter "msd_core*"
```

### 対処

- `.csproj` に以下を追加してビルド時に DLL をコピーする。

```xml
<ItemGroup>
  <Content Include="..\..\..\core\build\msd_core.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </Content>
</ItemGroup>
```

- DLL が存在しても失敗する場合は CPU アーキテクチャ（x64/ARM64）の不一致を疑う。

---

## 原因 2 — 全ケースの `Enabled` が `false`

### 原因

`RebuildPlot` は `entry.Enabled == false` のケースを無条件にスキップする。

```csharp
// MainWindowViewModel.cs:263
if (!entry.Enabled) continue;
```

CheckBox の初期値が `false` になっていたり、前のセッションの状態が残っていると全線が消える。

### 確認方法

左ペインの Case 一覧で全ケースの CheckBox にチェックが入っているか目視確認する。

### 対処

`CaseEntry` のデフォルト値を必ず `true` にする。

```csharp
// ViewModels/CaseEntry.cs
public bool Enabled { get; set; } = true;   // false にしない
```

---

## 原因 3 — シミュレーション結果が 0 サンプル

### 原因

`dt` が `stop` より大きいと `sim.T.Length == 0` になり、`pts` が空のまま `SeriesPolyline` に渡される。
空の `Points` リストでは `Polyline` は何も描画しない。

### 確認方法

- `dt` (NumericUpDown) と `stop` の値を確認する。
- StatusMessage が `"OK — N cases, 0 samples each"` なら該当。

### 対処

バリデーションを `Run()` の先頭に追加する。

```csharp
public void Run()
{
    if (Dt <= 0 || Stop <= 0 || Dt >= Stop)
    {
        StatusMessage = $"Error: dt={Dt:G4} must be > 0 and < stop={Stop:G4}";
        return;
    }
    // ...
}
```

---

## 原因 4 — Y レンジが縮退している

### 原因

全ケースの応答が定数（例: `x(t) = 0` のみ）のとき `yMax - yMin < 1e-12` になる。
コードはこれを検出してパディングを加えるが、`dy = yMax - yMin` が 0 に近いと
座標変換 `fy = (sim.X[i] - yMin) / dy` が `NaN` や `Inf` になる可能性がある。

```csharp
// MainWindowViewModel.cs:273
if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
```

### 確認方法

全ケースで `k` が非常に大きく `F ≈ 0` のとき（静的釣り合い点 = 0）に起きやすい。

### 対処（現状のコードで安全な追加確認）

```csharp
double dy = yMax - yMin;
if (!double.IsFinite(dy) || dy < 1e-12) dy = 1.0;   // 安全フォールバック
```

---

## 原因 5 — Avalonia の Polyline.Points バインディング型不整合

### 原因

Avalonia の `Polyline.Points` プロパティは **`AvaloniaList<Point>`** または `IList<Point>` を受け付ける。
`Points` に通常の `List<Point>` や配列を渡すとバインディングは成功するが、
**コレクション変更通知が届かない**ため UI が更新されないことがある。

### 現状のコード（正しい実装）

```csharp
// MainWindowViewModel.cs:323
var pts = new AvaloniaList<Point>();   // AvaloniaList を使っている → OK
```

### 注意点

`SeriesPolyline.Points` を `List<Point>` や `Point[]` に変更した場合は必ず `AvaloniaList<Point>` に戻す。

```csharp
// SeriesPolyline.cs — 変更禁止
public required AvaloniaList<Avalonia.Point> Points { get; init; }
```

---

## 原因 6 — ItemsControl の ItemsPanel が Canvas でない / 寸法不一致

### 原因

```xml
<!-- MainWindow.axaml:181-189 -->
<ItemsControl ItemsSource="{Binding Series}">
    <ItemsControl.ItemsPanel>
        <ItemsPanelTemplate>
            <Canvas Width="700" Height="460"/>   <!-- 外側 Canvas と同寸法にする -->
        </ItemsPanelTemplate>
    </ItemsControl.ItemsPanel>
```

`ItemsPanelTemplate` の Canvas 寸法が外側の `<Canvas Width="700" Height="460">` と異なると、
座標系がずれて全線がクリッピング領域外に出る。

### 確認方法

`PlotWidthPx = 700`, `PlotHeightPx = 460`（MainWindowViewModel 定数）と AXAML の Canvas 寸法が一致しているか確認する。

### 対処

3 か所の Canvas 寸法（外側 Canvas・GridLines 用 ItemsPanelTemplate・Series 用 ItemsPanelTemplate）を必ず同じ値にする。変更するときは 3 か所同時に変更する。

---

## 原因 7 — UI スレッド違反

### 原因

`RebuildPlot()` が非同期タスクや別スレッドから呼ばれると `AvaloniaList` への書き込みがスレッドセーフではなくなり、描画が反映されなかったり例外が飛ぶ。

### 確認方法

デバッグ実行時に `System.InvalidOperationException: "Call from invalid thread"` が出力ウィンドウに流れていないか確認する。

### 対処

`Run()` や `RebuildPlot()` は必ず UI スレッド（= Avalonia の Dispatcher スレッド）から呼ぶ。
非同期処理をする場合は以下のように Dispatcher に戻す。

```csharp
await Avalonia.Threading.Dispatcher.UIThread.InvokeAsync(() => RebuildPlot());
```

---

## 原因 8 — DataContext が設定されていない

### 原因

`App.axaml.cs` で `DataContext = new MainWindowViewModel()` が設定されているが、
コードを変更した際に `DataContext` の設定が失われると全バインディングが無効になる。

### 確認方法

```csharp
// App.axaml.cs — これが必ず存在することを確認
desktop.MainWindow = new MainWindow
{
    DataContext = new MainWindowViewModel(),
};
```

AXAML 側で `d:DataContext` を指定していた場合、ランタイムの DataContext が上書きされているか確認する。

---

## 原因 9 — `AvaloniaXamlLoader.Load` vs Source Generator

### 原因

現在の実装は `AvaloniaXamlLoader.Load(this)` を使用している（非 source-generator 方式）。
Avalonia 11 以降では source generator 方式（`InitializeComponent()` が自動生成）が推奨されており、
混在すると AXAML の変更がランタイムに反映されないことがある。

### 確認方法

`.csproj` に以下が含まれているか確認する。

```xml
<AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>
```

コンパイル済みバインディングを有効にした場合、`{Binding Stroke}` のような型なしバインディングはコンパイルエラーになるため、
`DataType` を明示するか `{ReflectionBinding}` を使う必要がある。

---

## デバッグ手順（再現時の確認フロー）

```
1. StatusMessage を確認
   → "Simulation failed"  → 原因 1（DLL / パラメータ）
   → "OK — 0 samples"     → 原因 3（dt/stop 設定）
   → "OK — N cases, ..."  → 2 以降を確認

2. Case リストのチェックボックスを確認
   → 全て unchecked       → 原因 2

3. デバッガで RebuildPlot() にブレークポイント
   → Series.Count を確認  → 0 なら原因 1/2/3
   → Points.Count を確認  → 0 なら原因 3/4
   → pts の座標値を確認   → NaN/Inf なら原因 4

4. Avalonia Diagnostic Overlay を有効化
   → AppBuilder に .With<AvaloniaDiagnostics>() を追加して視覚的に確認
```

---

## 変更時のチェックリスト

Polyline 関連のコードを変更するたびに以下を確認する。

- [ ] `SeriesPolyline.Points` の型が `AvaloniaList<Avalonia.Point>` のままか
- [ ] `Canvas Width/Height` が MainWindowViewModel の `PlotWidthPx`/`PlotHeightPx` と一致しているか
- [ ] `RebuildPlot()` が UI スレッドから呼ばれているか
- [ ] `msd_core.dll` がビルド出力ディレクトリに存在するか
- [ ] `Run()` 後に StatusMessage が `"OK"` になっているか
- [ ] ケースの `Enabled` が `true` のケースが最低 1 つあるか
