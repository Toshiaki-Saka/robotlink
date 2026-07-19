# Avalonia 11 実装時の注意事項

このプロジェクト（PID デモ / frontend_avalonia, pkg/pattern2_avalonia）で実際に踏んだ落とし穴と対処法のまとめ。

---

## 1. Canvas は固定サイズ — ウィンドウリサイズに追従しない

### 問題
`<Canvas Width="720" Height="320">` で固定するとグリッド線・目盛り位置をすべてピクセルでハードコードしなければならず、ウィンドウをリサイズしても描画領域は変わらない。

### 対処
カスタムコントロール (`Control` サブクラス) を作り `Render(DrawingContext ctx)` をオーバーライドする。
`Bounds.Width` / `Bounds.Height` でその時点のサイズを取得して描画する。

```csharp
public override void Render(DrawingContext ctx)
{
    double w = Bounds.Width;
    double h = Bounds.Height;
    if (w < 1 || h < 1) return;
    // ... w/h を使ってスケーリング
}
```

---

## 2. リサイズで再描画されない — BoundsProperty を AffectsRender に含める

### 問題
`AffectsRender<PlotControl>(SimTProperty, ...)` だけでは、ウィンドウリサイズ時に `Render()` が呼ばれない。

### 対処
`BoundsProperty` を **必ず** `AffectsRender` に追加する。

```csharp
static PlotControl()
{
    AffectsRender<PlotControl>(
        SimTProperty, SimThetaProperty,
        YMinProperty, YMaxProperty, XMaxProperty,
        ThetaGoalProperty, TitleProperty,
        BoundsProperty   // ← これがないとリサイズで再描画されない
    );
}
```

---

## 3. Polyline.Points には AvaloniaList\<Point\> が必要

### 問題
Canvas ベースの実装で `Polyline` の `Points` を `List<Point>` や `IEnumerable<Point>` にバインドしても更新が反映されない。

### 対処
`AvaloniaList<Point>` を使う。`Clear()` → `Add()` でインプレース更新すれば通知が走る。

```csharp
// ViewModel
public AvaloniaList<Point> ResponsePoints { get; } = new();

// Run() の中で
ResponsePoints.Clear();
for (int i = 0; i < sim.T.Length; ++i)
    ResponsePoints.Add(new Point(...));
```

---

## 4. Slider / NumericUpDown のバインドは Mode=TwoWay を明示する

### 問題
Avalonia のデフォルトバインディングモードは `OneWay` のため、Slider を動かしても ViewModel に値が伝わらない。

### 対処
スライダーや入力コントロールは必ず `Mode=TwoWay` を付ける。

```xml
<Slider Value="{Binding Kp, Mode=TwoWay}"/>
<NumericUpDown Value="{Binding Kp, Mode=TwoWay}"/>
```

---

## 5. HeaderedContentControl は使わない

### 問題
`HeaderedContentControl` でスライダーパネルのヘッダーを実装しようとしたが、Avalonia 11 の Fluent テーマではスタイルが適用されず見た目が崩れた。

### 対処
`TextBlock` + `Border` + `StackPanel` の組み合わせで代替する。

```xml
<StackPanel>
    <TextBlock Text="Parameters" FontWeight="Bold" Margin="2,0,0,2"/>
    <Border BorderBrush="Gray" BorderThickness="1" Padding="8" CornerRadius="2">
        <StackPanel Spacing="4">
            <!-- スライダー群 -->
        </StackPanel>
    </Border>
</StackPanel>
```

---

## 6. カスタムコントロールのプロパティは StyledProperty で登録する

### 問題
通常の CLR プロパティだとバインディングが機能せず、変更通知も `AffectsRender` に乗らない。

### 対処
`AvaloniaProperty.Register<TOwner, TValue>()` で StyledProperty を定義する。

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

## 7. Render() 内のポリラインは StreamGeometry で描く

### 問題
`Render()` 内で点列を描くとき、`Polyline` コントロールは使えない。

### 対処
`StreamGeometry` を開いて `BeginFigure` / `LineTo` / `EndFigure` で線を構築し `DrawGeometry` で描画する。

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

## 8. 破線は DashStyle で指定する

```csharp
var dashStyle = new DashStyle(new[] { 6.0, 4.0 }, 0);
ctx.DrawLine(
    new Pen(Brushes.Red, 1.4, dashStyle),
    new Point(x1, y), new Point(x2, y));
```

---

## 9. InitializeComponent() は AvaloniaXamlLoader.Load(this) を呼ぶ

source generator を使わない場合（`AvaloniaUseCompiledBindingsByDefault=false`）は手動で呼ぶ。

```csharp
private void InitializeComponent() => AvaloniaXamlLoader.Load(this);
```

コードビハインドのコンストラクタ内で必ず呼び出すこと。呼び忘れると AXAML の内容が反映されない。

---

## 10. ネイティブ DLL は csproj で条件コピーする

プラットフォームごとに DLL/SO/dylib の拡張子が異なるため、`Condition="Exists(...)"` で分岐して出力ディレクトリへコピーする。

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

## 11. AXAML の Line 要素をレジェンドに使うときは Width を明示する

`StackPanel` 内の `Line` は `Width` を設定しないとレイアウトが崩れる。

```xml
<Line Stroke="#D62828" StrokeThickness="2"
      StrokeDashArray="3,2"
      StartPoint="0,0" EndPoint="16,0"
      VerticalAlignment="Center" Width="16"/>
```

---

## 12. 数値フォーマット文字列は G4 / F3 など明示する

`FormattedText` や `NumericUpDown.FormatString` で書式を指定しないと、ロケールによって小数点記号がカンマになる場合がある。`CultureInfo.InvariantCulture` を常に渡す。

```csharp
var ft = new FormattedText(text, CultureInfo.InvariantCulture,
    FlowDirection.LeftToRight, Typeface.Default, 9, Brushes.Black);
```

---

## 推奨アーキテクチャ（このプロジェクトの最終形）

| 要素 | 推奨 |
|------|------|
| プロット描画 | `Control` サブクラス + `Render(DrawingContext)` |
| プロパティ | `StyledProperty` + `AffectsRender`（`BoundsProperty` 含む） |
| ViewModel → View のデータ | 生データ配列（ピクセル変換は View 側） |
| ポリライン | `StreamGeometry` |
| スライダー | `Mode=TwoWay` 必須 |
| グループUI | `TextBlock` + `Border` + `StackPanel`（HeaderedContentControl 不可） |
| ネイティブ DLL | csproj 条件コピー（Exists チェック） |
