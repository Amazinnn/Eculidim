# Eculidim 复杂函数积分与求导能力增强设计方案

**目标：** 使 Eculidim 能够计算多项式 × 三角、多项式 × 指数等需要反复分部积分的复杂不定积分；求导能力已完善，保持稳定。

**策略：** 积分公式优先 → Tabular 循环分部迭代 → Taylor 级数逐项积分兜底。

**状态：** 草稿 v0.1

---

## 一、现有能力评估

### 求导（`eculid_diff.c`）
已完善，覆盖：
- 乘积法则、链式法则、商法则
- 全部三角/双曲/反三角函数
- 指数、对数、根式
- 特殊函数（erf、digamma）
- `ec_diff_with_steps` 支持推导步骤

**无需修改。**

### 积分（`eculid_integrate.c`）
基础覆盖良好，存在以下缺口：

| 缺口 | 当前行为 | 期望行为 |
|------|---------|---------|
| `x^n·sin(ax)` | 单步分部，返回不完整表达式 | 循环迭代至多项式降为 0 |
| `x^n·e^(ax)` | 同上 | 同上 |
| `x^n·ln(x)` | 不支持 | Tabular 法 |
| `sin^n(x)` | 不支持 | 降幂公式 |
| `cos^m(x)·sin^n(x)` | 不支持 | 降幂公式 |
| `e^(ax)·sin(bx)` | 单步分部 | 联立方程法得出闭式解 |
| 无公式积分 | 返回 NULL | Taylor 级数逐项积分 |

---

## 二、三段式积分引擎架构

### 阶段 1：扩展积分公式表（`integ_rec`）

在现有 `integ_rec` 基础上**补充**以下 pattern，按优先级排序：

#### 1.1 三角幂 — 降幂公式

```
∫ sin^n(x) dx:
  n = 1  → -cos(x)
  n = 2  → x/2 - sin(2x)/4
  n odd → -cos(x)·S(n-1) + 降项
  n even → x/2 - sin(2x)/4 + 递归降幂

∫ cos^n(x) dx:
  n = 1  → sin(x)
  n = 2  → x/2 + sin(2x)/4
  n odd/even → 同上

∫ sin(ax)·cos(bx) dx → 和差化积后分别积分
```

#### 1.2 多项式 × 三角 — Tabular 法（下详）

#### 1.3 多项式 × 指数 — Tabular 法（下详）

#### 1.4 指数乘三角联立 — 复数法

```
∫ e^(ax)·sin(bx) dx → 取 Im[∫ e^(a+ib)x dx]
                      = e^(ax)·(a·sin(bx) - b·cos(bx)) / (a²+b²)
∫ e^(ax)·cos(bx) dx → 取 Re[...]
```

#### 1.5 对数幂

```
∫ x^n·ln(x) dx → 分部积分一步：
  u = ln(x), dv = x^n dx
  = x^(n+1)·ln(x)/(n+1) - ∫ x^n/(n+1) dx
```

### 阶段 2：Tabular 分部积分法（新增 `integ_tabular`）

**输入：** 乘积表达式 `A·B`，变量 `var`

**判断规则：**
- `A` 为多项式（`x`, `x²`, `x³`...）：行 A 表
- `B` 为指数类（`e^(kx)`, `sin(kx)`, `cos(kx)`）：行 B 表

**Tabular 表格构造：**

| 列 i | D 列（A 的第 i 次导数） | I 列（B 的第 i 次积分） |
|------|----------------------|----------------------|
| 0    | x³                   | sin(nx)             |
| 1    | 3x²                  | -cos(nx)/n          |
| 2    | 6x                   | -sin(nx)/n²         |
| 3    | 6                    | cos(nx)/n³          |
| 4    | 0                    | —                    |

**交替符号：** (+)(−)(+)(−)...

**结果：** Σ [ (−1)^i · D_i · I_(i+1) ]

**循环终止条件：**
- D 列导数为 0（多项式次数耗尽）→ 返回闭式解
- 超过最大迭代次数（20次）→ 放弃，转阶段 3

**Tabular 核心函数：**

```c
/* src/eculid_integrate.c */
static Expr* integ_tabular(const Expr *A, const Expr *B, char var) {
    /* 判断 A 是否为 var 的多项式，B 是否为可积超越函数 */
    /* 构造 D 列（多项式逐次求导）和 I 列（超越函数逐次积分）*/
    /* 累加 (-1)^i * D_i * I_(i+1) */
    /* 化简返回 */
}
```

**对外接口：** 在 `EC_MUL` case 中，`integ_by_parts` 替换为 `integ_tabular` 优先尝试。

### 阶段 3：Taylor 级数逐项积分兜底

**触发条件：** 阶段 1 和阶段 2 均返回 NULL。

**实现策略：** 在 `eculid_series.c` 中新增函数：

```c
/* src/eculid_series.c */
Expr* ec_series_integral(Expr *e, const char *var, int order) {
    /* 1. 在 var=0 处展开 f(x) = Σ c_k·x^k  至指定阶数 */
    Expr *series = ec_maclaurin(e, var, order);

    /* 2. 逐项积分：Σ c_k·x^(k+1)/(k+1) */
    Expr *result = integrate_series_termwise(series, var);

    /* 3. 化简并返回 */
    return ec_simplify(result);
}
```

**order 默认值：** 10（可通过 REPL 命令参数指定）

**精度说明：** 这是符号级数展开，不是数值积分。返回 `+C`，表示通式。

**积分步骤输出：** 阶段 3 在 derivation 中记录：
1. `f(x) = ...`（展开结果）
2. `∫ f(x)dx = ...`（逐项积分结果）

---

## 三、文件修改清单

| 文件 | 修改内容 |
|------|---------|
| `src/eculid_integrate.c` | 1. 新增 `integ_tabular` 函数<br>2. 新增三角幂降幂公式（`integ_trig_power`）<br>3. 新增指数×三角联立公式（`integ_exp_trig`）<br>4. `EC_MUL` case 优先调用 tabular<br>5. `ec_integrate_with_steps` 支持 tabular 推导步骤<br>6. 阶段 3 触发时调用级数积分 |
| `src/eculid_series.c` | 新增 `ec_series_integral` 函数（Taylor 逐项积分） |
| `src/test.c` | 新增回归测试用例 |
| `docs/error-codes.md` | 新增 EC013（分部积分迭代超限）|

---

## 四、新增测试用例

```c
/* Tabular 分部积分 */
{ "∫x³sin(x)dx",  "x³(-cos(x)) + 3x²(sin(x)) + 6x(cos(x)) - 6sin(x) + C" }
/* 三角降幂 */
{ "∫sin²(x)dx",   "x/2 - sin(2x)/4 + C" }
/* 指数×三角联立 */
{ "∫eˣsin(x)dx",  "eˣ(sin(x)-cos(x))/2 + C" }
/* 级数兜底 */
{ "∫e^(x²)dx",   "级数展开..." }
/* 高阶多项式×三角 */
{ "∫x⁴cos(2x)dx", tabular 结果 }
/* 对数幂 */
{ "∫x·ln(x)dx", "x²ln(x)/2 - x²/4 + C" }
```

---

## 五、REPL 语法

无变化。现有 `\int <expr>` 语法继续适用，内部自动选择最佳策略。

可选增强：添加 `\int <expr> order <N>` 用于控制级数展开阶数。

---

## 六、错误处理

| 错误码 | 含义 | 触发条件 |
|--------|------|---------|
| EC005 | 积分无解析解 | 所有策略均失败 |
| EC013 | 分部积分迭代超限 | Tabular 超过 20 次迭代 |
| EC011 | 级数收敛失败 | Taylor 展开在给定阶数内不收敛 |

EC005 已在 `eculid_main.c` 中处理为切换数值积分。

---

## 七、Spec Self-Review

1. **Placeholder 扫描：** 无 TBD/TODO，所有函数名和文件路径已明确。
2. **内部一致性：** 三阶段顺序清晰；tabular 输入/输出接口明确；series_integral 接收/返回表达式树。
3. **范围检查：** 专注于不定积分，不涉及多元/偏微分；求导保持不变。
4. **歧义检查：** "复杂函数"已在需求澄清中明确为三类；策略选择（公式→tabular→级数）无歧义。
