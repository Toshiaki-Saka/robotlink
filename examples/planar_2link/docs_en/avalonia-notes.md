# Avalonia 11 implementation notes

A collection of the pitfalls we actually hit in this project (the PID demo / frontend_avalonia, pkg/pattern2_avalonia) and how to work around them.

---

## 1. Canvas has a fixed size — it does not follow window resizing

### Problem
If you pin it with `<Canvas Width="720" Height="320">`, you have to hardcode every grid line and tick position in pixels, and the drawing area does not change even when the window is resized.

### Fix
Create a custom control (a `Control` subclass) and override `Render(DrawingContext ctx)`.
Get the current size from `Bounds.Width` / `Bounds.Height` and draw accordingly.

```csharp
public override void Render(DrawingContext ctx)
{
    double w = Bounds.Width;
    double h = Bounds.Height;
    if (w < 1 || h < 1) return;
    // ... scale using w/h
}
```

---

## 2. No redraw on resize — include BoundsProperty in AffectsRender

### Problem
With only `AffectsRender<PlotControl>(SimTProperty, ...)`, `Render()` is not called when the window is resized.

### Fix
**Always** add `BoundsProperty` to `AffectsRender`.

```csharp
static PlotControl()
{
    AffectsRender<PlotControl>(
        SimTProperty, SimThetaProperty,
        YMinProperty, YMaxProperty, XMaxProperty,
        ThetaGoalProperty, TitleProperty,
        BoundsProperty   // ← without this, there is no redraw on resize
    );
}
```

---

## 3. Polyline.Points requires an AvaloniaList\<Point\>

### Problem
In a Canvas-based implementation, binding `Polyline`'s `Points` to a `List<Point>` or `IEnumerable<Point>` does not reflect updates.

### Fix
Use `AvaloniaList<Point>`. If you update in place with `Clear()` → `Add()`, the change notification fires.

```csharp
// ViewModel
public AvaloniaList<Point> ResponsePoints { get; } = new();

// inside Run()
ResponsePoints.Clear();
for (int i = 0; i < sim.T.Length; ++i)
    ResponsePoints.Add(new Point(...));
```

---

## 4. Bindings for Slider / NumericUpDown must specify Mode=TwoWay explicitly

### Problem
Avalonia's default binding mode is `OneWay`, so moving the Slider does not propagate the value to the ViewModel.

### Fix
Always add `Mode=TwoWay` to sliders and input controls.

```xml
<Slider Value="{Binding Kp, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding Kp, Mode=TwoWay}"/>
```

---

## 5. Do not use HeaderedContentControl

### Problem
We tried to implement the slider panel header with `HeaderedContentControl`, but under Avalonia 11's Fluent theme the styles were not applied and the layout broke.

### Fix
Substitute a combination of `TextBlock` + `Border` + `StackPanel`.

```xml
<StackPanel>
    <TextBlock Text="Parameters" FontWeight="Bold" Margin="2,0,0,2"/>
    <Border BorderBrush="Gray" BorderThickness="1" Padding="8" CornerRadius="2">
        <StackPanel Spacing="4">
            <!-- slider group -->
        </StackPanel>
    </Border>
</StackPanel>
```

---

## 6. Register custom control properties as StyledProperty

### Problem
With a plain CLR property, binding does not work and change notifications are not picked up by `AffectsRender`.

### Fix
Define a StyledProperty with `AvaloniaProperty.Register<TOwner, TValue>()`.

```csharp
public static readonly StyledProperty<double[]?> SimTProperty =
    AvaloniaProperty.Register<PlotControl, double[]?>(nameof(SimT));

public double[]? SimT
{
    get => GetValue(SimTProperty);
    set => SetValue(SimTProperty, value);
}
```

---

## 7. Draw polylines inside Render() with StreamGeometry

### Problem
When drawing a sequence of points inside `Render()`, you cannot use the `Polyline` control.

### Fix
Open a `StreamGeometry`, build the line with `BeginFigure` / `LineTo` / `EndFigure`, and draw it with `DrawGeometry`.

```csharp
var geo = new StreamGeometry();
using (var gc = geo.Open())
{
    gc.BeginFigure(ToPx(t[0], th[0]), false);
    for (int i = 1; i < t.Length; ++i)
        gc.LineTo(ToPx(t[i], th[i]));
    gc.EndFigure(false);
}
ctx.DrawGeometry(null, new Pen(Brushes.Blue, 1.6), geo);
```

---

## 8. Specify dashed lines with DashStyle

```csharp
var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
ctx.DrawLine(
    new Pen(Brushes.Red, 1.4, dashStyle),
    new Point(x1, y), new Point(x2, y));
```

---

## 9. InitializeComponent() must call AvaloniaXamlLoader.Load(this)

If you are not using the source generator (`AvaloniaUseCompiledBindingsByDefault=false`), call it manually.

```csharp
private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
```

Be sure to call it inside the code-behind constructor. If you forget, the AXAML content is not applied.

---

## 10. Copy native DLLs conditionally in the csproj

Because the DLL/SO/dylib extension differs per platform, branch with `Condition="Exists(...)"` and copy to the output directory.

```xml
<ItemGroup Condition="Exists('..\..\core\build\pid_core.dll')">
    <None Include="..\..\core\build\pid_core.dll">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
<ItemGroup Condition="Exists('..\..\core\build\libpid_core.so')">
    <None Include="..\..\core\build\libpid_core.so">
        <CopyToOutputDirectory>PreserveNewest</CopyToOutputDirectory>
    </None>
</ItemGroup>
```

---

## 11. When using an AXAML Line element in a legend, specify Width explicitly

A `Line` inside a `StackPanel` breaks the layout unless you set `Width`.

```xml
<Line Stroke="#D62828" StrokeThickness="2"
      StrokeDashArray="3,2"
      StartPoint="0,0" EndPoint="16,0"
      VerticalAlignment="Center" Width="16"/>
```

---

## 12. Specify a numeric format string such as G4 / F3 explicitly

If you do not specify a format in `FormattedText` or `NumericUpDown.FormatString`, the decimal separator may become a comma depending on the locale. Always pass `CultureInfo.InvariantCulture`.

```csharp
var ft = new FormattedText(text, CultureInfo.InvariantCulture,
    FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
```

---

## Recommended architecture (the final shape of this project)

| Element | Recommendation |
|------|------|
| Plot rendering | `Control` subclass + `Render(DrawingContext)` |
| Properties | `StyledProperty` + `AffectsRender` (including `BoundsProperty`) |
| ViewModel → View data | Raw data arrays (pixel conversion on the View side) |
| Polyline | `StreamGeometry` |
| Slider | `Mode=TwoWay` required |
| Group UI | `TextBlock` + `Border` + `StackPanel` (HeaderedContentControl not allowed) |
| Native DLL | Conditional copy in csproj (Exists check) |
