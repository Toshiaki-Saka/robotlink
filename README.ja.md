# RobotLink

[English](README.md) | 📖 **日本語**

3自由度の空間ロボットアームを対象とした、**記号的導出 → C++コード生成 → シミュレーション → 可視化**。

RobotLinkは、3自由度アームの完全な非線形運動方程式を[SymPy](https://www.sympy.org/)で記号的に導出し、最適化されたC++ヘッダとしてエクスポートしたうえで、計算トルク制御によるシミュレーションを[Eigen](https://eigen.tuxfamily.org/)を用いてC++で実行します。結果はCSVに書き出され、インタラクティブなPython製ビジュアライザーで確認できます。

![Arm tracking demo](docs_en/arm_animation.gif)

### このデモが何をしているのか

上のアームは、記録された動きを再生しているのではありません。**大きくずれた姿勢から
始めて、指令された軌道に追従しよう**としています。各関節にはそれぞれ独立した正弦波が
指令されており (ベースの旋回 ±34°、肩のピッチ、肘はほぼ伸びきった状態と直角の間で
屈伸)、それらが同時に、しかも*異なる周波数*で動きます。そのため手先は繰り返しのない
3 次元の軌跡を描き、アームが対称な姿勢に落ち着くことはありません。これには意味が
あります。周波数をずらした同時運動こそが、速度に依存するコリオリ・遠心力項と姿勢に
依存する質量行列を実行中ずっと働かせ続けるからです — まさに手計算では厄介な項であり、
本プロジェクトがそれらを記号的に導出している理由でもあります。

さらにシミュレーションは、アームを**あるべき位置から 0.57 m ずれた状態**(肘の誤差
1.2 rad) から開始します。したがって最初の 0.5 秒ほどは収束の過渡応答として目に見え、
その後は計算トルク制御が積分の丸め誤差レベルで目標に追従し続けます。つまりこのデモは、
*生成された動力学は正しいか*と、*コントローラは初期誤差をオーバーシュートなく消し、
その後厳密に追従できるか*という 2 つの問いに同時に答えています。

軌道パラメータの詳細、それぞれを選んだ理由、良い結果の見分け方、自分の実験への
作り変え方は [`docs_ja/demo-scenario.md`](docs_ja/demo-scenario.md) にまとめてあります。

> **付属サンプル — `examples/planar_2link`。** 空間3自由度アームの依存関係が軽量な**平面2リンク**版の兄弟実装です（旧`tlm`リポジトリから取り込み、git履歴を保持）。小さなC ABIコアを介して順運動学と到達可能作業領域の境界曲線を提供します。このルートからビルドできます（Eigenも記号的生成も不要）:
>
> ```sh
> cmake -S . -B build && cmake --build build --target tlm_core
> ctest --test-dir build -R planar_2link_python --output-on-failure
> cd gui/python && pip install -r requirements.txt
> python gallery_app.py --example planar_2link
> ```
>
> `-DROBOTLINK_BUILD_PLANAR_2LINK=OFF` で無効化できます。

---

## このプロジェクトの狙い

多くの教材は、(a) 動力学を手書きするか（誤りが生じやすく拡張も難しい）、あるいは (b) すべてをPythonで完結させ、リアルタイム動作可能なコードには決して到達しないか、のいずれかです。RobotLinkはその代わりに、完全なパイプラインを示します:

1. ラグランジュ動力学を記号的に**導出**する（`M`、`C`、`g` を手書きしない）。
2. 共通部分式除去により、分岐のないC++ヘッダを**生成**する。
3. 固定ステップRK4積分器と計算トルク制御で**シミュレーション**する。
4. 関節の追従、トルク、エンドエフェクタの経路を**可視化**する。

記号的処理はビルド時に一度だけ実行され、生成されるC++は実行時にSymPyへの依存を持ちません。

## リポジトリ構成

```
.
├── CMakeLists.txt              # ビルド + 動力学生成のオーケストレーション
├── include/                    # ヘッダオンリーC++（アーム、コントローラ、RK4、設定）
│   ├── sim_config.hpp          # アーム/シミュレーション/コントローラ/軌道のパラメータ
│   ├── robot_arm.hpp           # 動力学 + 順運動学
│   ├── controller.hpp          # 計算トルクコントローラ
│   └── rk4.hpp                 # 汎用4次ルンゲ・クッタ積分器
├── src/main.cpp                # シミュレーションのエントリポイント
├── python/
│   └── derive_and_export.py    # SymPyによる導出 → generated/dynamics_generated.hpp
├── frontend_python/            # 結果ビューア（デフォルト: matplotlib）
│   ├── visualizer.py           # tkinter + matplotlib 結果ビューア（デフォルト）
│   ├── visualizer_matplotlib.py
│   ├── visualizer_pyqt6.py
│   └── visualizer_pyside.py
├── frontend_qt/                # Qt6 C++ ビジュアライザー（CMake）
├── frontend_avalonia/          # Avalonia C# ビジュアライザー（.NET 8）
├── tests/                      # 物理ベースの回帰テスト（CTest）
│   ├── test_harness.hpp        # 依存関係のない小さなテストフレームワーク
│   ├── test_rk4.cpp            # 積分器 vs. 閉形式解
│   └── test_dynamics.cpp       # 質量行列、重力、コントローラの性質
├── docs_en/                    # 英語ドキュメント（+ 共有画像）
│   ├── demo-scenario.md        # デモの運動が何で、なぜそれを選んだのか
│   ├── dynamics.md             # 一般化座標、ラグランジアン、運動方程式の導出、制御
│   ├── integration-notes.md    # RK4 vs. 適応型ソルバ — それぞれの適用場面
│   ├── arm_animation.gif       # デモアニメーション
│   └── tracking_plots.png      # 追従プロットのサンプル
├── docs_ja/                    # 日本語ドキュメント（日本語版）
│   ├── demo-scenario.md
│   ├── dynamics.md
│   └── integration-notes.md
├── generated/                  # （ビルド出力）自動生成されるC++ヘッダ
└── output/                     # （実行出力）sim_results.csv
```

`generated/` と `output/` はビルド/実行によって生成されるもので、意図的にgit-ignoreされています。

## 必要環境

- **CMake** ≥ 3.20
- **C++20** コンパイラ（GCC 11+、Clang 13+、または MSVC 2022）
- `sympy` を備えた **Python 3.9+**（ビルド時のみ）
- **Eigen 3.4** — CMakeが自動的に取得するため、手動インストールは不要
- ビジュアライザー用: `numpy`、`pandas`、`matplotlib`（およびTk対応のPython）

```bash
pip install sympy                       # ビルドに必須
pip install numpy pandas matplotlib     # ビジュアライザーにのみ必要
```

## ビルドと実行

```bash
# 1. 構成（初回ビルド時に動力学ヘッダも導出します — 約1〜3分かかります）
cmake -B build -S .

# 2. ビルド（シミュレーションとテストスイートをコンパイル）
cmake --build build

# 3. テストの実行
ctest --test-dir build --output-on-failure

# 4. シミュレーションの実行（output/sim_results.csv を書き出します）
./build/robot_sim                       # Linux/macOS
# build\Release\robot_sim.exe           # Windows / MSVC

# 5. 可視化
python frontend_python/visualizer.py
```

初回の構成/ビルドは、`derive_and_export.py` が記号的なオイラー・ラグランジュ導出を実行するため時間がかかります。スクリプトが変更されない限り、以降のビルドではキャッシュされたヘッダが再利用されます。

### Windows PowerShell ヘルパースクリプト

Windowsでは、上記の手順をラップする2つのPowerShellスクリプトが用意されており、手作業で実行する必要がありません。

**`run_sim.ps1`** — ビルド、テスト、シミュレーション実行、そしてデフォルト（matplotlib）ビジュアライザーの起動を一括で行います:

```powershell
.\run_sim.ps1                   # 通常のビルド + テスト + 実行 + 可視化
.\run_sim.ps1 -Clean            # build/ を削除して完全な再ビルドを行う
.\run_sim.ps1 -SkipTests        # CTestの実行をスキップする
.\run_sim.ps1 -SkipVisualize    # ビルドと実行のみ、ビジュアライザーなし
.\run_sim.ps1 -UseSystemEigen   # FetchContentの代わりにインストール済みEigenを使う
```

**`build_and_run.ps1`** — コアをビルドして実行し、3つのGUIフロントエンド（Qt6 C++、Avalonia C#、PyQt6 Python）のうち正確に1つを起動します:

```powershell
.\build_and_run.ps1                       # デフォルト（Qt6）、フル実行
.\build_and_run.ps1 1                     # Qt6 (C++)       frontend_qt
.\build_and_run.ps1 2                     # Avalonia (C#)   frontend_avalonia
.\build_and_run.ps1 3                     # PyQt6 (Python)  frontend_python
.\build_and_run.ps1 3 -SkipBuild -SkipSim # 既存のCSVを使い、ビジュアライザーのみ起動する
.\build_and_run.ps1 1 -BuildType Debug    # Qt6 / Debug ビルド
.\build_and_run.ps1 -Clean                # 完全な再ビルド
```

Qt6とAvaloniaのフロントエンドは事前にビルドしておく必要があります（`build_and_run.ps1` はそれらを起動するだけです）。PyQt6は事前ビルドが不要です。詳細は各スクリプト冒頭のヘッダコメントを参照してください。

### ビルドオプション

| オプション                     | デフォルト | 効果                                        |
|--------------------------------|---------|-----------------------------------------------|
| `ROBOTLINK_BUILD_TESTS`        | `ON`    | CTestテストスイートをビルドして登録する       |
| `ROBOTLINK_USE_SYSTEM_EIGEN`   | `OFF`   | FetchContentの代わりにインストール済みEigen3を使う（オフラインビルド向け） |

システムのEigenに対するオフラインビルドの例:

```bash
cmake -B build -S . -DROBOTLINK_USE_SYSTEM_EIGEN=ON
```

## 仕組み

### 1. 記号的動力学（`python/derive_and_export.py`）

アームは、3つの関節で駆動される2つの剛体リンクとしてモデル化されます。スクリプトはリンクの重心位置と角速度を構築し、運動エネルギーと位置エネルギーを求め、オイラー・ラグランジュ方程式

```
d/dt (∂L/∂q̇) − ∂L/∂q = τ
```

を適用して、質量行列 $M(q)$、コリオリ/遠心項 $C(q, \dot{q}) \cdot \dot{q}$、および重力ベクトル $g(q)$ を得ます。共通部分式除去（`sympy.cse`）が結果を圧縮し、それが `generated/dynamics_generated.hpp` 内の `compute_dynamics(...)` として出力されます。

導出の全体 — 一般化座標、運動学モデル、運動/位置エネルギー、オイラー・ラグランジュ方程式、構造化された運動方程式 — は [`docs_ja/dynamics.md`](docs_ja/dynamics.md) に文書化されています。

### 2. 計算トルク制御（`include/controller.hpp`）

コントローラは非線形動力学をキャンセルし、線形な誤差ダイナミクスを課します:

```
τ = M(q)·[q̈_d + Kd·(q̇_d − q̇) + Kp·(q_d − q)] + C(q,q̇)·q̇ + g(q)
```

これにより閉ループ誤差方程式 $\ddot{e} + K_d \cdot \dot{e} + K_p \cdot e = 0$ が得られます。 $K_p = 80$、 $K_d = 18$ のとき、閉ループ極は $s = -8, -10$ にあります（臨界制動、オーバーシュートなし）。

### 3. 積分（`include/rk4.hpp`）

汎用の固定ステップ4次ルンゲ・クッタ積分器が状態 $[q, \dot{q}]$ を前進させます。アームには接触や切り替えイベントがないため、解は滑らかなまま保たれ、固定ステップ（`dt = 0.005 s`）でも十分な精度が得られます — RK4と適応型の議論については [`docs_ja/integration-notes.md`](docs_ja/integration-notes.md) を参照してください。

## テスト

テストスイートは、小さなヘッダオンリーのハーネス（`tests/test_harness.hpp`、外部依存なし）を用い、記号的導出がどのように発展しても成り立たなければならない性質を検証します:

- **`test_rk4`** — 積分器が指数減衰、調和振動子、線形ランプをそれぞれの閉形式解に対して再現し、振動子のエネルギーを保存すること。
- **`test_dynamics`** — 質量行列が対称かつ正定値であること、静的姿勢が $\tau = g(q)$ によって正確に釣り合うこと、順動力学が運動方程式を反転すること、そして計算トルクコントローラが追従誤差をゼロへ収束させること。

これらは `ctest --test-dir build --output-on-failure` で実行します。

## 設定

シミュレーションのパラメータは `include/sim_config.hpp` にあります: リンク長と質量、時間ステップ、制御ゲイン、および目標軌道 $q_{d,i}(t) = offset_i + A_i \cdot \sin(w_i \cdot t + \varphi_i)$。編集して再ビルドすることで実験できます — 現在の値が何を狙って選ばれているか、また試す価値のある変更例は [`docs_ja/demo-scenario.md`](docs_ja/demo-scenario.md) を参照してください。

## ロードマップ

- [x] 物理ベースの回帰テスト（エネルギー保存、質量行列の対称性、重力の釣り合い）
- [ ] `derive_and_export.py` を任意の開連鎖リンク数に一般化する
- [ ] CSV出力向けのブラウザベース3Dビューア
- [ ] 逆運動学のデモ

## コントリビューション

コントリビューションを歓迎します — [CONTRIBUTING.md](CONTRIBUTING.md) を参照してください。

## ライセンス

Apache License 2.0 のもとでライセンスされています。[LICENSE](LICENSE) を参照してください。
