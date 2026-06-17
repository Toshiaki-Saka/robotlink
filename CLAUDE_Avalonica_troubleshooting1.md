# CLAUDE.md — tdof プロジェクト

## プロジェクト構成

- `core/` — C++ コアライブラリ（計算エンジン）
- `frontend_avalonia/` — Avalonia 11 (.NET) フロントエンド
- `frontend_qt/` — Qt フロントエンド（リファレンス実装）
- `frontend_python/` — Python フロントエンド

---

## Avalonia 11 注意事項（繰り返し発生した問題）

### 1. HeaderedContentControl は使わない
FluentTheme での動作が不安定でパラメータが表示されないことがある。
**代わりに必ず以下の明示的な構造を使う：**
```xml
<StackPanel>
    <TextBlock Text="ヘッダー名" />
    <Border BorderBrush="Gray" BorderThickness="1">
        <StackPanel>
            <!-- 内容 -->
        </StackPanel>
    </Border>
</StackPanel>
```

### 2. Canvas のグリッド線は Line 要素で明示的に描く
`Grid.ShowGridLines` 等は Canvas では機能しない。
**必ず Line 要素を XAML または コードビハインドで追加する：**
- 水平5本・垂直5本
- Avalonia 11 の正しい API: `StartPoint` / `EndPoint` プロパティを使う（`X1/Y1/X2/Y2` ではない）
- プロット枠は `Rectangle` で追加

```xml
<Line StartPoint="0,100" EndPoint="300,100" Stroke="Gray" StrokeThickness="1"/>
```

### 3. 目盛りラベルは ViewModel に専用プロパティを持つ
Y軸・X軸それぞれ5点（0%, 25%, 50%, 75%, 100%）のラベルを ViewModel に定義し、
`TextBlock` で表示する。Qt フロントエンドの見た目をリファレンスにする。

必要なプロパティ例：
- Y軸: `RefY0`, `RefY25`, `RefY50`, `RefY75`, `RefY100`
- X軸: `RefX0`, `RefX25`, `RefX50`, `RefX75`, `RefX100`

### 4. Binding エラーは Silent に失敗する
XAML の Binding ミス（プロパティ名の typo 等）はエラーを出さず表示されないだけ。
デバッグ時は `INotifyPropertyChanged` の発火と Property 名の一致を必ず確認する。

---

## Qt フロントエンドとの対応関係
Avalonia 実装を進めるときは `frontend_qt/` の実装を常にリファレンスとして参照する。
見た目・動作・データ構造はなるべく Qt 版に合わせる。
