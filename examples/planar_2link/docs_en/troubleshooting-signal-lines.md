# Troubleshooting: Signal Lines (Polyline) Not Displaying

This document systematically summarizes the causes and fixes for cases where the Polyline is not rendered in the Series `ItemsControl` of `MainWindow.axaml`.

---

## Symptom Checklist

| Symptom | First cause to suspect |
|---|---|
| Grid lines appear but only the signal lines are missing | Simulation failure / `Enabled=false` / empty Points |
| Neither grid nor signal lines appear at all | DataContext not set / Native DLL not found |
| Appears at first but disappears after Run | Inconsistency in the `_sims` dictionary / thread violation |
| Missing only for some cases | Simulation exception in an individual case |
| Appears right after startup but disappears after changing parameters | The `SyncEditorToCase` → `Run` chain is broken |

---

## Cause 1 — Native DLL not found / simulation failure

### Cause

If `MsdCoreNative.Simulate` inside `MsdSolver.Simulate` returns `IntPtr.Zero` or throws an exception,
the catch block in `Run()` sets `_sims[entry] = null`.
Because `RebuildPlot` skips `null` entries, **no lines are rendered at all**.

```csharp
// MsdSolver.cs:94-96
var handle = MsdCoreNative.Simulate(ref cn, ref sn);
if (handle == IntPtr.Zero)
    throw new InvalidOperationException("msd_core_simulate failed");
```

### How to check

1. Check the **StatusMessage** at the bottom of the window.
   - If `"Simulation failed for N case(s)"` is shown, the problem is on the DLL side.
2. Check whether `msd_core.dll` (or `libmsd_core.so`) exists in the build output directory.

```powershell
# Check the output folder after building
Get-ChildItem "bin\Debug\net8.0\" -Filter "msd_core*"
```

### Fix

- Add the following to the `.csproj` so the DLL is copied at build time.

```xml
<ItemGroup>
  <Content Include="..\..\..\core\build\msd_core.dll">
    <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
  </Content>
</ItemGroup>
```

- If it still fails even though the DLL exists, suspect a CPU architecture (x64/ARM64) mismatch.

---

## Cause 2 — `Enabled` is `false` for all cases

### Cause

`RebuildPlot` unconditionally skips cases where `entry.Enabled == false`.

```csharp
// MainWindowViewModel.cs:263
if (!entry.Enabled) continue;
```

If the CheckBox's initial value is `false`, or if state from a previous session persists, all lines disappear.

### How to check

Visually verify in the Case list in the left pane that the CheckBox is checked for every case.

### Fix

Always set the default value of `CaseEntry` to `true`.

```csharp
// ViewModels/CaseEntry.cs
public bool Enabled { get; set; } = true;   // do not set to false
```

---

## Cause 3 — Simulation result has 0 samples

### Cause

If `dt` is larger than `stop`, `sim.T.Length == 0`, and `pts` is passed to `SeriesPolyline` while still empty.
With an empty `Points` list, the `Polyline` renders nothing.

### How to check

- Check the values of `dt` (NumericUpDown) and `stop`.
- Applies if the StatusMessage is `"OK — N cases, 0 samples each"`.

### Fix

Add validation at the beginning of `Run()`.

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

## Cause 4 — Y range is degenerate

### Cause

When the response of every case is constant (e.g. `x(t) = 0` only), `yMax - yMin < 1e-12`.
The code detects this and adds padding, but if `dy = yMax - yMin` is close to 0,
the coordinate transform `fy = (sim.X[i] - yMin) / dy` may become `NaN` or `Inf`.

```csharp
// MainWindowViewModel.cs:273
if (yMax - yMin < 1e-12) { yMin -= 0.5; yMax += 0.5; }
```

### How to check

Most likely to occur when `k` is very large and `F ≈ 0` for every case (static equilibrium point = 0).

### Fix (a safe additional check for the current code)

```csharp
double dy = yMax - yMin;
if (!double.IsFinite(dy) || dy < 1e-12) dy = 1.0;   // safe fallback
```

---

## Cause 5 — Avalonia Polyline.Points binding type mismatch

### Cause

Avalonia's `Polyline.Points` property accepts **`AvaloniaList<Point>`** or `IList<Point>`.
If you pass an ordinary `List<Point>` or an array to `Points`, the binding succeeds, but
because **collection change notifications are not delivered**, the UI may not update.

### Current code (correct implementation)

```csharp
// MainWindowViewModel.cs:323
var pts = new AvaloniaList<Point>();   // using AvaloniaList → OK
```

### Note

If you change `SeriesPolyline.Points` to `List<Point>` or `Point[]`, always change it back to `AvaloniaList<Point>`.

```csharp
// SeriesPolyline.cs — do not change
public required AvaloniaList<Avalonia.Point> Points { get; init; }
```

---

## Cause 6 — The ItemsControl's ItemsPanel is not a Canvas / dimension mismatch

### Cause

```xml
<!-- MainWindow.axaml:181-189 -->
<ItemsControl ItemsSource="{Binding Series}">
    <ItemsControl.ItemsPanel>
        <ItemsPanelTemplate>
            <Canvas Width="700" Height="460"/>   <!-- make the same size as the outer Canvas -->
        </ItemsPanelTemplate>
    </ItemsControl.ItemsPanel>
```

If the Canvas dimensions in the `ItemsPanelTemplate` differ from the outer `<Canvas Width="700" Height="460">`,
the coordinate systems become misaligned and all lines fall outside the clipping region.

### How to check

Verify that `PlotWidthPx = 700`, `PlotHeightPx = 460` (constants in MainWindowViewModel) match the Canvas dimensions in the AXAML.

### Fix

Always set the three Canvas dimensions (the outer Canvas, the ItemsPanelTemplate for GridLines, and the ItemsPanelTemplate for Series) to the same value. When you change them, change all three at once.

---

## Cause 7 — UI thread violation

### Cause

If `RebuildPlot()` is called from an asynchronous task or another thread, writes to the `AvaloniaList` are no longer thread-safe, so the rendering may not be reflected or an exception may be thrown.

### How to check

While debugging, check whether `System.InvalidOperationException: "Call from invalid thread"` is appearing in the Output window.

### Fix

Always call `Run()` and `RebuildPlot()` from the UI thread (i.e. Avalonia's Dispatcher thread).
When doing asynchronous processing, return to the Dispatcher as follows.

```csharp
await Avalonia.Threading.Dispatcher.UIThread.InvokeAsync(() => RebuildPlot());
```

---

## Cause 8 — DataContext is not set

### Cause

`DataContext = new MainWindowViewModel()` is set in `App.axaml.cs`, but
if the `DataContext` assignment is lost when the code is changed, all bindings become invalid.

### How to check

```csharp
// App.axaml.cs — make sure this is always present
desktop.MainWindow = new MainWindow
{
    DataContext = new MainWindowViewModel(),
};
```

If you specified `d:DataContext` on the AXAML side, check whether the runtime DataContext is being overwritten.

---

## Cause 9 — `AvaloniaXamlLoader.Load` vs Source Generator

### Cause

The current implementation uses `AvaloniaXamlLoader.Load(this)` (the non-source-generator approach).
Since Avalonia 11, the source generator approach (where `InitializeComponent()` is auto-generated) is recommended,
and mixing the two can cause AXAML changes not to be reflected at runtime.

### How to check

Check whether the `.csproj` contains the following.

```xml
<AvaloniaUseCompiledBindingsByDefault>true</AvaloniaUseCompiledBindingsByDefault>
```

If you enable compiled bindings, untyped bindings such as `{Binding Stroke}` become compile errors,
so you need to either specify `DataType` explicitly or use `{ReflectionBinding}`.

---

## Debugging Procedure (verification flow when reproducing)

```
1. Check StatusMessage
   → "Simulation failed"  → Cause 1 (DLL / parameters)
   → "OK — 0 samples"     → Cause 3 (dt/stop settings)
   → "OK — N cases, ..."  → Check Cause 2 onward

2. Check the checkboxes in the Case list
   → all unchecked        → Cause 2

3. Set a breakpoint at RebuildPlot() in the debugger
   → Check Series.Count   → if 0, Cause 1/2/3
   → Check Points.Count   → if 0, Cause 3/4
   → Check the coordinate values of pts → if NaN/Inf, Cause 4

4. Enable the Avalonia Diagnostic Overlay
   → Add .With<AvaloniaDiagnostics>() to the AppBuilder to verify visually
```

---

## Checklist When Making Changes

Verify the following every time you change Polyline-related code.

- [ ] Is the type of `SeriesPolyline.Points` still `AvaloniaList<Avalonia.Point>`?
- [ ] Do `Canvas Width/Height` match MainWindowViewModel's `PlotWidthPx`/`PlotHeightPx`?
- [ ] Is `RebuildPlot()` called from the UI thread?
- [ ] Does `msd_core.dll` exist in the build output directory?
- [ ] Is the StatusMessage `"OK"` after `Run()`?
- [ ] Is there at least one case whose `Enabled` is `true`?
