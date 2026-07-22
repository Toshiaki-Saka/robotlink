# 動力学と運動方程式

本ドキュメントでは、RobotLink の 3 自由度ロボットアームについて、完全な運動方程式を導出する。
一般化座標の定義から始まり、ラグランジュ形式とオイラー・ラグランジュ方程式を経て、計算トルク制御器と数値積分に至るまでを扱う。

すべての式は `python/derive_and_export.py`（記号的導出）、`include/sim_config.hpp`（パラメータ）、および `include/controller.hpp`（制御則）に直接対応している。

---

## 1. 一般化座標

このアームは 3 個の回転関節を持つ。一般化座標ベクトルは次のとおりである。

$$\mathbf{q} = \begin{pmatrix} q_1 \\ q_2 \\ q_3 \end{pmatrix} \in \mathbb{R}^3, \qquad \dot{\mathbf{q}} = \frac{d\mathbf{q}}{dt}, \qquad \ddot{\mathbf{q}} = \frac{d^2\mathbf{q}}{dt^2}$$

| 記号 | 関節タイプ | 軸 | 物理的意味 |
|--------|-----------|------|-----------------|
| $q_1$ | 回転 | ワールド $z$ | ヨー — アーム全体を水平面内で回転させる |
| $q_2$ | 回転 | $R_z(q_1)\,\hat{y}$ | ショルダーピッチ — リンク 2 を鉛直面内で上下させる |
| $q_3$ | 回転 | $R_z(q_1)\,\hat{y}$ | エルボーピッチ — リンク 3 をリンク 2 に対して曲げる |

単位はラジアンである。関節 2 と関節 3 は（ヨー $q_1$ を適用した後の）同一のピッチ軸を共有するため、これは **ヨー–ピッチ–ピッチ** 構成である。

---

## 2. 回転行列

用いる基本回転行列は次の 2 つである。

$$R_z(\theta) = \begin{pmatrix} \cos\theta & -\sin\theta & 0 \\ \sin\theta & \cos\theta & 0 \\ 0 & 0 & 1 \end{pmatrix}, \qquad R_y(\theta) = \begin{pmatrix} \cos\theta & 0 & \sin\theta \\ 0 & 1 & 0 \\ -\sin\theta & 0 & \cos\theta \end{pmatrix}$$

ワールド座標系における各リンクの姿勢は次のとおりである。

$$R_{12} = R_z(q_1)\,R_y(q_2), \qquad R_{123} = R_z(q_1)\,R_y(q_2+q_3)$$

関節 2 と関節 3 はどちらも同一の軸 $R_z(q_1)\hat{y}$ まわりに回転するため、$R_{123}$ ではそれらのピッチ角は単純に加算される。

---

## 3. 運動学

### 3.1 重心位置

各リンクは **一様な棒** としてモデル化され、その重心はローカル $x$ 軸に沿った中点に位置する。$\hat{x} = (1,0,0)^\top$ とする。

**リンク 2**（長さ $L_2$、質量 $m_2$）:

$$\mathbf{p}_{c2} = R_{12}\,\frac{L_2}{2}\,\hat{x} = \frac{L_2}{2} \begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix}$$

**リンク 3**（長さ $L_3$、質量 $m_3$）: リンク 3 の原点はリンク 2 の末端にある。

$$\mathbf{p}_{c3} = R_{12}\,L_2\,\hat{x} + R_{123}\,\frac{L_3}{2}\,\hat{x} = L_2\begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix} + \frac{L_3}{2}\begin{pmatrix} \cos q_1\cos(q_2+q_3) \\ \sin q_1\cos(q_2+q_3) \\ -\sin(q_2+q_3) \end{pmatrix}$$

### 3.2 順運動学（エンドエフェクタ）

関節位置（`robot_arm.hpp` に実装）:

$$\mathbf{p}_\text{elbow} = L_2 \begin{pmatrix} \cos q_1\cos q_2 \\ \sin q_1\cos q_2 \\ -\sin q_2 \end{pmatrix}$$

$$\mathbf{p}_\text{hand} = \mathbf{p}_\text{elbow} + L_3 \begin{pmatrix} \cos q_1\cos(q_2+q_3) \\ \sin q_1\cos(q_2+q_3) \\ -\sin(q_2+q_3) \end{pmatrix}$$

### 3.3 重心速度

時間で微分すると:

$$\dot{\mathbf{p}}_{c2} = \frac{d}{dt}\left[R_{12}\,\frac{L_2}{2}\,\hat{x}\right], \qquad \dot{\mathbf{p}}_{c3} = \frac{d}{dt}\left[R_{12}\,L_2\,\hat{x} + R_{123}\,\frac{L_3}{2}\,\hat{x}\right]$$

これらは SymPy によって記号的に計算される（`sp.diff(p_c2, t)`、`sp.diff(p_c3, t)`）。

### 3.4 角速度

ワールド座標系における 2 つの関節軸を次のように定義する。

$$\hat{z} = \begin{pmatrix}0\\0\\1\end{pmatrix}, \qquad \hat{y}_\text{rot} = R_z(q_1)\begin{pmatrix}0\\1\\0\end{pmatrix} = \begin{pmatrix}-\sin q_1\\\cos q_1\\0\end{pmatrix}$$

$\hat{z} \perp \hat{y}_\text{rot}$（正規直交対）であることに注意する。

各リンクの角速度は、そのリンクまでの（そのリンクを含む）すべての関節からの寄与を合計して組み立てられる。

$$\boldsymbol{\omega}_2 = \dot{q}_1\,\hat{z} + \dot{q}_2\,\hat{y}_\text{rot}$$

$$\boldsymbol{\omega}_3 = \dot{q}_1\,\hat{z} + (\dot{q}_2+\dot{q}_3)\,\hat{y}_\text{rot}$$

リンクの **長手軸**（ワールド座標系における各棒の長さ方向）は次のとおりである。

$$\hat{a}_2 = R_{12}\,\hat{x} = \begin{pmatrix}\cos q_1\cos q_2\\\sin q_1\cos q_2\\-\sin q_2\end{pmatrix}, \qquad \hat{a}_3 = R_{123}\,\hat{x} = \begin{pmatrix}\cos q_1\cos(q_2+q_3)\\\sin q_1\cos(q_2+q_3)\\-\sin(q_2+q_3)\end{pmatrix}$$

---

## 4. ラグランジアン

ラグランジアンは $\mathcal{L} = T - U$ である。

### 4.1 運動エネルギー

各リンクの重心の **並進運動エネルギー**:

$$T_{\text{trans},k} = \frac{1}{2}\,m_k\,\dot{\mathbf{p}}_{ck}^\top\dot{\mathbf{p}}_{ck}, \qquad k = 2,3$$

**回転運動エネルギー**: 質量 $m$、長さ $L$ の一様な棒に対して、重心まわりの慣性テンソルは横方向モーメント $I_\perp = mL^2/12$ を持ち、軸方向モーメントはゼロである（棒自身の軸まわりのスピンは無視する）。したがって回転運動エネルギーは次のとおりである。

$$T_{\text{rot}} = \frac{1}{2}\,I_\perp\,\|\boldsymbol{\omega}_\perp\|^2 = \frac{1}{2}\cdot\frac{mL^2}{12}\cdot\left(\|\boldsymbol{\omega}\|^2 - (\boldsymbol{\omega}\cdot\hat{a})^2\right) = \frac{mL^2}{24}\left(\|\boldsymbol{\omega}\|^2 - (\boldsymbol{\omega}\cdot\hat{a})^2\right)$$

ここで $\boldsymbol{\omega}_\perp = \boldsymbol{\omega} - (\boldsymbol{\omega}\cdot\hat{a})\hat{a}$ は棒に垂直な $\boldsymbol{\omega}$ の成分である。

**全運動エネルギー**:

$$T = \frac{1}{2}m_2\,\dot{\mathbf{p}}_{c2}^\top\dot{\mathbf{p}}_{c2} + \frac{m_2 L_2^2}{24}\!\left(\|\boldsymbol{\omega}_2\|^2 - (\boldsymbol{\omega}_2\cdot\hat{a}_2)^2\right) + \frac{1}{2}m_3\,\dot{\mathbf{p}}_{c3}^\top\dot{\mathbf{p}}_{c3} + \frac{m_3 L_3^2}{24}\!\left(\|\boldsymbol{\omega}_3\|^2 - (\boldsymbol{\omega}_3\cdot\hat{a}_3)^2\right)$$

$T$ は $\dot{\mathbf{q}}$ について 2 次形式であるため、常に次のように書ける。

$$T = \frac{1}{2}\,\dot{\mathbf{q}}^\top M(\mathbf{q})\,\dot{\mathbf{q}}$$

ここで $M(\mathbf{q})$ は構成に依存する対称正定値行列である。

### 4.2 位置エネルギー

ワールド $z$ 軸を上向きとし、アームの基部を $z = 0$ とすると:

$$U = m_2\,g\,(\mathbf{p}_{c2})_z + m_3\,g\,(\mathbf{p}_{c3})_z$$

3.1 節の $z$ 成分を代入すると:

$$U = -\frac{m_2\,g\,L_2}{2}\sin q_2 + m_3\,g\!\left(-L_2\sin q_2 - \frac{L_3}{2}\sin(q_2+q_3)\right)$$

$$\boxed{U = -\left(\frac{m_2}{2}+m_3\right)g L_2\sin q_2 - \frac{m_3\,g\,L_3}{2}\sin(q_2+q_3)}$$

$U$ は $q_1$ に依存しないことに注意する — ヨー回転はリンクの高さを変えない。

---

## 5. オイラー・ラグランジュ方程式

一般化座標 $q_i$ に対するオイラー・ラグランジュ方程式は次のとおりである。

$$\frac{d}{dt}\!\left(\frac{\partial\mathcal{L}}{\partial\dot{q}_i}\right) - \frac{\partial\mathcal{L}}{\partial q_i} = \tau_i, \qquad i = 1,2,3$$

$\mathcal{L} = T - U$ を展開し、$\partial U/\partial\dot{q}_i = 0$ を用いると:

$$\frac{d}{dt}\!\left(\frac{\partial T}{\partial\dot{q}_i}\right) - \frac{\partial T}{\partial q_i} + \frac{\partial U}{\partial q_i} = \tau_i$$

SymPy による導出（`derive_and_export.py`）では、これは次のように計算される。

```python
raw = sp.diff(sp.diff(Lag, dqi), t) - sp.diff(Lag, qi)
EOM[i] = sp.trigsimp(raw)
```

---

## 6. 運動方程式

運動学的な式を展開し、$\ddot{\mathbf{q}}$、$\dot{\mathbf{q}}\otimes\dot{\mathbf{q}}$、および $\mathbf{q}$ のみの部分ごとに項をまとめると、標準的なロボット動力学の形式が得られる。

$$\boxed{M(\mathbf{q})\,\ddot{\mathbf{q}} + C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} + \mathbf{g}(\mathbf{q}) = \boldsymbol{\tau}}$$

### 6.1 質量行列 $M(\mathbf{q})$

$M(\mathbf{q}) \in \mathbb{R}^{3\times3}$ は $\ddot{\mathbf{q}}$ の係数として抽出される。

$$M_{ij}(\mathbf{q}) = \frac{\partial\,[\text{EOM}_i]}{\partial\ddot{q}_j}$$

コードでは:
```python
M_mat[i, j] = sp.trigsimp(sp.diff(EOM_i[i], DDQ[j]))
```

**性質**（`tests/test_dynamics.cpp` で検証済み）:

- **対称性**: $M = M^\top$、すなわち $M_{ij} = M_{ji}$
- **正定値性**: すべての $\mathbf{v}\neq\mathbf{0}$ に対して $\mathbf{v}^\top M\,\mathbf{v} > 0$
- **構成依存性**: $q_2$ と $q_3$ のみが現れ、$q_1$ は $M$ に入らない
  （ヨー軸まわりのアームの慣性はどのヨー角でも同じである）

対角要素は各関節に対する実効慣性を表し、非対角要素は関節間の動的な結合を表す。

### 6.2 コリオリ・遠心力項 $C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}}$

コリオリ・遠心力ベクトル $\mathbf{c} = C\dot{\mathbf{q}} \in \mathbb{R}^3$ は、オイラー・ラグランジュ方程式のうち $\ddot{\mathbf{q}}$ を含まず $\dot{\mathbf{q}}$ に依存する部分である。

$$c_i = \left.[\text{EOM}_i]\right|_{\ddot{\mathbf{q}}=\mathbf{0}} - g_i = \sum_{j,k}\Gamma_{ijk}\,\dot{q}_j\dot{q}_k$$

ここで $\Gamma_{ijk}$ は第 1 種クリストッフェル記号である。

$$\Gamma_{ijk} = \frac{1}{2}\!\left(\frac{\partial M_{ij}}{\partial q_k} + \frac{\partial M_{ik}}{\partial q_j} - \frac{\partial M_{jk}}{\partial q_i}\right)$$

コードでは:
```python
rest  = EOM evaluated with ddq = 0
g_vec = rest evaluated with dq  = 0
Cqdq  = rest - g_vec
```

**エネルギー整合性**: 上記のクリストッフェル記号の定義により、行列 $C$ は $\dot{M} - 2C$ が歪対称になるように選ぶことができる。これは次を保証する。

$$\dot{\mathbf{q}}^\top C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} = 0$$

これはコリオリ・遠心力がシステムに正味の仕事をしないことを意味する — 内部結合力に対するニュートンの第 3 法則の帰結である。

### 6.3 重力ベクトル $\mathbf{g}(\mathbf{q})$

$\mathbf{g} \in \mathbb{R}^3$ は位置エネルギーの勾配である。

$$g_i(\mathbf{q}) = \frac{\partial U}{\partial q_i}$$

4.2 節の式を微分すると:

$$g_1 = 0$$

$$g_2 = -\left(\frac{m_2}{2}+m_3\right)g L_2\cos q_2 - \frac{m_3\,g\,L_3}{2}\cos(q_2+q_3)$$

$$g_3 = -\frac{m_3\,g\,L_3}{2}\cos(q_2+q_3)$$

$g_1 = 0$ となるのは、鉛直軸まわりの回転（$q_1$ は純粋なヨー）がどのリンクの高さも変えないためである。

**静的平衡のチェック**: $\boldsymbol{\tau} = \mathbf{g}(\mathbf{q})$、$\dot{\mathbf{q}} = \ddot{\mathbf{q}} = \mathbf{0}$ とすると、運動方程式が恒等的に満たされる。これは `tests/test_dynamics.cpp` でテストされている。

---

## 7. 順動力学

与えられた印加トルク $\boldsymbol{\tau}$ から、関節加速度を解く。

$$\ddot{\mathbf{q}} = M(\mathbf{q})^{-1}\!\left[\boldsymbol{\tau} - C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} - \mathbf{g}(\mathbf{q})\right]$$

`robot_arm.hpp` では $M$ が正定値であるため、コレスキー分解が用いられる。

```cpp
d.M.llt().solve(tau - d.Cdq - d.gv)
```

積分に用いる完全な 6 次元状態ベクトルは次のとおりである。

$$\mathbf{x} = \begin{pmatrix}\mathbf{q}\\\dot{\mathbf{q}}\end{pmatrix} \in \mathbb{R}^6, \qquad \dot{\mathbf{x}} = f(\mathbf{x},t) = \begin{pmatrix}\dot{\mathbf{q}}\\M(\mathbf{q})^{-1}\!\left[\boldsymbol{\tau}(t,\mathbf{q},\dot{\mathbf{q}}) - C\dot{\mathbf{q}} - \mathbf{g}\right]\end{pmatrix}$$

---

## 8. 計算トルク制御

### 8.1 制御則

計算トルク（逆動力学）制御器は、アームの非線形動力学を打ち消し、望ましい線形の誤差ダイナミクスを課す。次のように定義する。

$$\mathbf{e}(t) = \mathbf{q}_d(t) - \mathbf{q}(t)$$

制御則は次のとおりである。

$$\boxed{\boldsymbol{\tau} = M(\mathbf{q})\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right] + C(\mathbf{q},\dot{\mathbf{q}})\,\dot{\mathbf{q}} + \mathbf{g}(\mathbf{q})}$$

ここで $K_p = k_p I_3$ および $K_d = k_d I_3$ はスカラーゲイン行列である。

### 8.2 閉ループ誤差ダイナミクス

制御則を $M\ddot{\mathbf{q}} + C\dot{\mathbf{q}} + \mathbf{g} = \boldsymbol{\tau}$ に代入する。

$$M\ddot{\mathbf{q}} + C\dot{\mathbf{q}} + \mathbf{g} = M\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right] + C\dot{\mathbf{q}} + \mathbf{g}$$

$C\dot{\mathbf{q}}$ 項と $\mathbf{g}$ 項は厳密に打ち消し合う（この打ち消しは動力学を正確に把握していることに依存する — 計算トルク制御器は **厳密線形化** の一形態である）。

$$M\ddot{\mathbf{q}} = M\!\left[\ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}\right]$$

$M$ は可逆であるため:

$$\ddot{\mathbf{q}} = \ddot{\mathbf{q}}_d + K_d\dot{\mathbf{e}} + K_p\mathbf{e}$$

$\ddot{\mathbf{e}} = \ddot{\mathbf{q}}_d - \ddot{\mathbf{q}}$ を用いると:

$$\boxed{\ddot{\mathbf{e}} + K_d\,\dot{\mathbf{e}} + K_p\,\mathbf{e} = \mathbf{0}}$$

非線形なアーム動力学は、**線形・非結合・時不変** なシステムに完全に置き換えられた。

### 8.3 極配置

$K_p = k_p I$ かつ $K_d = k_d I$ であるため、誤差ダイナミクスは 3 つの同一なスカラーシステムに分離する。各関節の特性多項式は次のとおりである。

$$s^2 + k_d\,s + k_p = 0$$

デフォルトゲイン $k_p = 80$、$k_d = 18$ では:

$$s^2 + 18s + 80 = (s+8)(s+10) = 0 \implies s_{1,2} = -8,\;-10$$

**過減衰応答**（追従誤差に振動なし）:

$$\Delta = k_d^2 - 4k_p = 324 - 320 = 4 > 0$$

閉ループの時定数は $\tau_1 = 1/8 = 0.125\,\text{s}$ および $\tau_2 = 1/10 = 0.1\,\text{s}$ である。

ステップ外乱に対して、誤差はおおよそ次のように減衰する。

$$\|\mathbf{e}(t)\| \sim C_1\,e^{-8t} + C_2\,e^{-10t}$$

### 8.4 閉ループ極における RK4 の安定性

固定ステップの RK4 積分器は、実負の固有値に対して $|\lambda\,\Delta t| \lesssim 2.79$ を要求する安定領域を持つ。$\Delta t = 0.005\,\text{s}$ では:

| 極 | $\lvert\lambda\,\Delta t\rvert$ | RK4 安定か? |
|------|----------------------|-------------|
| $s = -8$ | $0.040$ | はい — 余裕 $70\times$ |
| $s = -10$ | $0.050$ | はい — 余裕 $56\times$ |

RK4 に関する完全な議論については [`integration-notes.md`](integration-notes.md) を参照のこと。

---

## 9. 目標軌道

関節 $i$ の目標軌道は、オフセット付きの正弦波である。

$$q_{d,i}(t) = \delta_i + A_i\sin(\omega_i\,t + \varphi_i)$$

$$\dot{q}_{d,i}(t) = A_i\,\omega_i\cos(\omega_i\,t + \varphi_i)$$

$$\ddot{q}_{d,i}(t) = -A_i\,\omega_i^2\sin(\omega_i\,t + \varphi_i)$$

デフォルトのパラメータ値（`include/sim_config.hpp` より）:

| パラメータ | 関節 1 | 関節 2 | 関節 3 |
|-----------|--------:|--------:|--------:|
| 振幅 $A_i$ [rad] | 0.6 | 0.5 | 0.7 |
| 周波数 $\omega_i$ [rad/s] | 0.8 | 1.0 | 1.2 |
| 位相 $\varphi_i$ [rad] | $0$ | $\pi/4$ | $\pi/2$ |
| オフセット $\delta_i$ [rad] | $0$ | $-0.3$ | $0.8$ |

---

## 10. RK4 積分の要約

常微分方程式 $\dot{\mathbf{x}} = f(\mathbf{x}, t)$ は、固定ステップの 4 次ルンゲ・クッタ法（`include/rk4.hpp`）で前進させられ、1 ステップあたり 4 回の評価を用いる。

$$k_1 = f(\mathbf{x}_n,\,t_n)$$

$$k_2 = f\!\left(\mathbf{x}_n + \tfrac{\Delta t}{2}k_1,\; t_n + \tfrac{\Delta t}{2}\right)$$

$$k_3 = f\!\left(\mathbf{x}_n + \tfrac{\Delta t}{2}k_2,\; t_n + \tfrac{\Delta t}{2}\right)$$

$$k_4 = f\!\left(\mathbf{x}_n + \Delta t\,k_3,\; t_n + \Delta t\right)$$

$$\mathbf{x}_{n+1} = \mathbf{x}_n + \frac{\Delta t}{6}\!\left(k_1 + 2k_2 + 2k_3 + k_4\right)$$

大域打ち切り誤差は $O(\Delta t^4)$ である。$\Delta t = 0.005\,\text{s}$ で $T = 8\,\text{s}$ にわたって積分すると、積分器は 1,600 ステップを踏み、$f$ をちょうど 6,400 回呼び出す。

---

## 11. パラメータ一覧

物理パラメータは `include/sim_config.hpp` で定義されている。

| 記号 | 値 | 説明 |
|--------|------:|-------------|
| $L_2$ | 0.5 m | リンク 2 の長さ |
| $L_3$ | 0.4 m | リンク 3 の長さ |
| $m_2$ | 1.0 kg | リンク 2 の質量 |
| $m_3$ | 0.7 kg | リンク 3 の質量 |
| $g$ | 9.81 m/s² | 重力加速度 |
| $k_p$ | 80 | 比例ゲイン（関節ごと） |
| $k_d$ | 18 | 微分ゲイン（関節ごと） |
| $\Delta t$ | 0.005 s | RK4 ステップ幅 |
| $T_\text{end}$ | 8.0 s | シミュレーション時間 |
