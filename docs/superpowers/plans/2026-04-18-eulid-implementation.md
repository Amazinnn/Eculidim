# Eculidim 实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一个面向 AI Agent 的 C 语言数学工具 Eculidim，支持多模式有状态 CLI、JSON/文本双输出格式、状态持久化（内存存 AST + 文件存 LaTeX）、英文错误码。

**Architecture:** 模块化多文件架构，核心层（AST/解析/求值）与 CLI 层（多模式 REPL）分离，全局符号表在所有模式间共享。输出层分 JSON 引擎和文本引擎两套。

**Tech Stack:** 纯 C99，gcc/MinGW，无外部库依赖（仅 math.h）。

---

## 关键设计决策（已确认）

| 决策 | 选择 |
|------|------|
| 运行方式 | 双击 exe 或 PowerShell 启动 REPL，`--json` 自动进入 JSON 模式 |
| 交互对象 | 人和 Agent 共用同一套 REPL，Agent 通过 stdin/stdout 调用 |
| 状态管理 | 内存存 AST（求值快）+ 文件存 LaTeX（人可读），`exit` 自动保存最近 5 次 |
| 输出格式 | JSON 模式（Agent 默认） + 文本模式（人读），`\json`/`\text` 切换 |
| 提示符 | `ec> diff> int> solve> series> sum> limit> num>`，无 `[]` |
| 定界 | JSON 模式：完整 JSON 对象；文本模式：末尾附 `---END---` |
| 错误处理 | 全部英文，附错误码（EC001-EC012），详见 `docs/error-codes.md` |

---

## 文件结构

```
Eculidim/
├── include/
│   └── eculid.h           # 统一头文件
├── src/
│   ├── eculid_main.c      # 主入口 + REPL 总控 + JSON 输出引擎
│   ├── eculid_ast.c/h     # AST 节点定义
│   ├── eculid_lexer.c/h   # LaTeX 词法分析
│   ├── eculid_parser.c/h  # 递归下降解析
│   ├── eculid_simplify.c/h # 化简与变换
│   ├── eculid_diff.c/h    # 符号求导
│   ├── eculid_integrate.c/h # 符号积分
│   ├── eculid_solve.c/h   # 方程求解
│   ├── eculid_limit.c/h   # 极限
│   ├── eculid_series.c/h  # 级数展开
│   ├── eculid_sumprod.c/h # 求和与连乘
│   ├── eculid_eval.c/h    # 统一求值引擎
│   ├── eculid_latex.c/h   # LaTeX 解析与输出
│   ├── eculid_validator.c/h # 验证器
│   ├── eculid_plugin.c/h  # 插件注册
│   ├── eculid_session.c/h # 会话管理 + 状态持久化
│   ├── eculid_symtab.c/h  # 全局符号表（函数/变量）
│   └── eculid_util.c/h    # 工具函数
├── docs/
│   └── error-codes.md     # 错误码指南（Agent 查阅）
├── Makefile
└── README.md
```

---

## API 契约

### 公共类型

```c
/*============================================================
 * Expr — 表达式 AST 节点
 *============================================================*/
typedef enum {
    EC_NIL, EC_NUM, EC_VAR, EC_ADD, EC_SUB, EC_MUL, EC_DIV,
    EC_POW, EC_NEG, EC_MOD,
    /* 三角函数 */
    EC_SIN, EC_COS, EC_TAN, EC_COT, EC_SEC, EC_CSC,
    /* 反三角函数 */
    EC_ASIN, EC_ACOS, EC_ATAN, EC_ACOT, EC_ASEC, EC_ACSC,
    /* 双曲三角函数 */
    EC_SINH, EC_COSH, EC_TANH, EC_COTH, EC_SECH, EC_SCH,
    /* 反双曲三角函数 */
    EC_ASINH, EC_ACOSH, EC_ATANH, EC_ACOTH, EC_ASECH, EC_ACSCH,
    /* 指数与对数 */
    EC_EXP, EC_EXP2, EC_EXPM1,
    EC_LN, EC_LOG, EC_LOG10, EC_LOG2, EC_LN1P,
    /* 幂函数与根 */
    EC_SQRT, EC_CBRT, EC_HYPOT,
    /* 绝对值与符号 */
    EC_ABS, EC_SIGN,
    /* 取整函数 */
    EC_FLOOR, EC_CEIL, EC_ROUND, EC_TRUNC,
    /* 最大最小 */
    EC_MAX, EC_MIN,
    /* 伽马与阶乘 */
    EC_GAMMA, EC_FACTORIAL, EC_LGAMMA,
    /* 误差函数 */
    EC_ERF, EC_ERFC, EC_ERFI,
    /* 贝塞尔函数 */
    EC_J0, EC_J1, EC_JN, EC_Y0, EC_Y1, EC_YN,
    /* 特殊常数函数 */
    EC_DIGAMMA, EC_POLYGAMMA,
    /* 比较与逻辑 */
    EC_LT, EC_LE, EC_GT, EC_GE, EC_EQ, EC_NE,
    EC_LOR, EC_LAND, EC_LNOT,
    /* 微积分 */
    EC_INT, EC_DERIV, EC_PDIFF, EC_LIMIT,
    EC_SUM, EC_PROD, EC_SERIES,
    /* 常量与特殊 */
    EC_I, EC_PI, EC_E, EC_INF, EC_NINF,
    EC_COMP, EC_REM, EC_FDIM
} ECNodeType;

typedef struct Expr {
    ECNodeType type;
    double num_val;
    char   var_name;
    char   *var_str;
    struct Expr *left;
    struct Expr *right;
    struct Expr *arg;
    struct Expr *cond;
    int    deriv_order;
    char   deriv_var;
} Expr;

/*============================================================
 * Complex — 复数
 *============================================================*/
typedef struct { double re; double im; } Complex;

/*============================================================
 * DerivationStep — 推导步骤
 *============================================================*/
typedef struct { char *step_latex; char *rule; } DerivationStep;
typedef struct { DerivationStep *steps; int count; int capacity; } Derivation;

/*============================================================
 * 变量绑定与求值环境
 *============================================================*/
typedef struct { const char *name; double value; } ECVarBinding;
typedef struct { ECVarBinding *vars; int n; } ECEnv;

/*============================================================
 * 求值结果
 *============================================================*/
typedef enum {
    EC_VAL_SYMBOLIC, EC_VAL_REAL, EC_VAL_COMPLEX,
    EC_VAL_UNDEFINED, EC_VAL_INF, EC_VAL_ERROR
} ECValType;

typedef struct {
    ECValType type;
    double real, imag;
    Expr *sym;
    char *error;
} ECValue;

/*============================================================
 * 方程求解结果
 *============================================================*/
typedef struct { Expr **roots; int count; char *method; } EculidRoots;
typedef struct { Expr *lo, *hi; int inclusive; } ECInterval;

/*============================================================
 * EculidPlugin — 插件接口
 *============================================================*/
typedef struct {
    const char *name;
    const char *latex_prefix;
    Expr* (*apply)(const char *input, Expr *ast, void *ctx);
    void (*free_ctx)(void *ctx);
    const char *description;
} EculidPlugin;

/*============================================================
 * ValidationResult — 验证结果
 *============================================================*/
typedef struct { int valid; char msg[256]; int err_pos; } ValidationResult;

/*============================================================
 * 会话状态
 *============================================================*/
#define EC_MAX_HISTORY 256
typedef struct {
    char *input;
    char *output;
    char *operation;
} EculidHistoryEntry;

typedef struct {
    EculidHistoryEntry entries[EC_MAX_HISTORY];
    int count;
    int saved_count;        /* 已保存到文件的记录数（最多5） */
    Expr *ans;
    int steps_enabled;
    int json_mode;          /* JSON 输出模式 */
    ECSymTab *symtab;
} EculidSession;

/*============================================================
 * 错误码定义（供 Agent 查阅 docs/error-codes.md）
 *============================================================*/
typedef enum {
    EC_ERR_NONE        = 0,
    EC_ERR_PARSE       = 1,   /* EC001: LaTeX 解析错误 */
    EC_ERR_UNDEF_VAR   = 2,   /* EC002: 未定义的变量 */
    EC_ERR_UNSUP_CMD   = 3,   /* EC003: 不支持的 LaTeX 命令 */
    EC_ERR_DIV_ZERO    = 4,   /* EC004: 除零错误 */
    EC_ERR_NO_INT      = 5,   /* EC005: 积分无解析解 */
    EC_ERR_NO_SOLVE    = 6,   /* EC006: 方程无解 */
    EC_ERR_DEF_SYNTAX  = 7,   /* EC007: 定义语法错误 */
    EC_ERR_MODE        = 8,   /* EC008: 模式切换错误 */
    EC_ERR_IO          = 9,   /* EC009: 文件 I/O 错误 */
    EC_ERR_NO_STEPS    = 10,  /* EC010: 推导步骤生成失败 */
    EC_ERR_NO_CONV     = 11,  /* EC011: 级数展开收敛失败 */
    EC_ERR_BAD_ARG     = 12,  /* EC012: 无效的命令参数 */
} ECErrorCode;

typedef struct {
    ECErrorCode code;
    char message[256];
} ECError;
```

### 错误码指南摘要（`docs/error-codes.md` 包含完整说明）

```markdown
# Eculidim 错误码指南

## 概述

Eculidim 所有错误均通过英文 `error` 字段返回，格式如下：

JSON 模式：
```json
{
  "result": null,
  "steps": null,
  "error": {
    "code": "EC001",
    "message": "Parse error at position N: unexpected token 'TOKEN'"
  }
}
```

文本模式：
```
Error EC001: Parse error at position N: unexpected token 'TOKEN'
```

## 错误码列表

| 错误码 | 常量 | 含义 | 示例 |
|--------|------|------|------|
| EC001 | EC_ERR_PARSE | LaTeX 解析错误 | `\frac{}{}` 缺少分子 |
| EC002 | EC_ERR_UNDEF_VAR | 变量未定义 | 输入 `x + 1`，`x` 无定义 |
| EC003 | EC_ERR_UNSUP_CMD | 不支持的命令 | `\zeta(x)` 未支持 |
| EC004 | EC_ERR_DIV_ZERO | 除零 | `1/0` |
| EC005 | EC_ERR_NO_INT | 积分无解析解 | `\int e^{x^2} dx` |
| EC006 | EC_ERR_NO_SOLVE | 方程无解 | `x^2 + 1 = 0`（实数范围）|
| EC007 | EC_ERR_DEF_SYNTAX | 定义语法错误 | `\def f() = x` 缺参数 |
| EC008 | EC_ERR_MODE | 模式错误 | 在 diff> 下使用 num> 命令 |
| EC009 | EC_ERR_IO | 文件 I/O 错误 | 状态文件不可写 |
| EC010 | EC_ERR_NO_STEPS | 推导步骤不可用 | 当前表达式无法生成步骤 |
| EC011 | EC_ERR_NO_CONV | 级数收敛失败 | `\sum 1/n` 发散 |
| EC012 | EC_ERR_BAD_ARG | 参数错误 | `series x at x=inf order -1` |

## Agent 使用建议

1. 解析 JSON 响应时，**始终先检查 `error` 字段是否为 `null`**
2. 收到 EC005 时，Eculidim 会自动切换到数值积分，结果仍返回
3. 收到 EC006 时，可尝试 `--numeric` 模式求数值解
4. 重试时携带完整上下文（变量定义）避免 EC002
```

---

## 实现 Task 清单

---

### Task 1: 目录结构与编译验证

**Files:**
- Create: `Eculidim/Makefile`（MinGW gcc，含 `-lm`）
- Create: `Eculidim/include/eculid.h`
- Modify: `Eculidim/src/eculid_ast.h`（添加 `#include <stdio.h>`）
- Create: `Eculidim/src/` 所有 `.c/.h` 空壳文件（14个模块 + test.c + eculid_main.c）
- Create: `Eculidim/docs/error-codes.md`

- [ ] **Step 1: 修复已知编译问题**

修改 `src/eculid_ast.h`，在顶部添加：
```c
#include <stddef.h>
#include <stdio.h>
```

修改 `src/eculid_util.h`，移除 `ec_free` 重复声明。

修改 `src/eculid_util.c`，移除 `ec_free` 定义（由 AST 模块提供）。

- [ ] **Step 2: 编译验证**

Run: `export PATH="/c/Program Files (x86)/Dev-Cpp/MinGW32/bin:$PATH" && cd Eculidim && gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim -lm`

Expected: 编译通过，`Eculidim.exe` 生成，无警告

- [ ] **Step 3: 创建错误码文档**

写入 `docs/error-codes.md`（上述完整错误码指南）。

- [ ] **Step 4: 提交**

---

### Task 2: AST 节点定义与基础操作

**Files:**
- Modify: `Eculidim/include/eculid.h`
- Modify: `Eculidim/src/eculid_ast.c/h`

- [ ] **Step 1: 写入所有公共类型**

`ECNodeType`（约70种）、`Expr`、`Complex`、`DerivationStep`、`Derivation`、`ECVarBinding`、`ECEnv`、`ECValue`、`ECValType`、`EculidRoots`、`ECInterval`、`EculidPlugin`、`ValidationResult`、`EculidHistoryEntry`、`EculidSession`、`ECSymType`、`ECSymEntry`、`ECSymTab`、`ECErrorCode`、`ECError`。

- [ ] **Step 2: 实现全部构造函数**

`ec_num`, `ec_varc`, `ec_vars`, `ec_unary`, `ec_binary`, `ec_ternary`, `ec_const`, `ec_deriv`, `ec_intg`, `ec_defintg`, `ec_limit_node`, `ec_sum_node`, `ec_product_node`, `ec_series_node`。

- [ ] **Step 3: 实现复制/释放/查询/代入**

`ec_copy`, `ec_free`（处理 var_str 释放）, `ec_is_zero/one/const/number/integer/negative/complex`, `ec_has_var`, `ec_has_var_str`, `ec_substitute`, `ec_substitute_val`, `ec_substitute_func`, `ec_print`, `ec_fprint`。

- [ ] **Step 4: 编译验证**

Expected: `-Wall -Wextra` 无警告

---

### Task 3: 词法分析器

**Files:**
- Modify: `Eculidim/src/eculid_lexer.c/h`

- [ ] **Step 1: 实现 LaTeX Tokenizer**

识别数字、变量、运算符、`\cmd` 命令、`{ } ( ) [ ]`、下标 `_` 上标 `^`、逻辑运算符。空格跳过。

- [ ] **Step 2: 实现 peek/rewind**

`lex_peek` 返回当前 Token 但不前移。`lex_rewind` 撤销一个 Token。

- [ ] **Step 3: 验证**

```c
Lexer L; lex_init(&L, "\\frac{x^2}{3} + \\sin(y)");
Token t;
while ((t = lex_next(&L)).type != TK_EOF)
    printf("[%s:%s] ", token_type_name(t.type), t.text);
```
Expected: 输出所有 Token 序列

---

### Task 4: 递归下降解析器

**Files:**
- Modify: `Eculidim/src/eculid_parser.c/h`

- [ ] **Step 1: 实现优先级解析**

```
parse_expr   → parse_addsub
parse_addsub → parse_muldiv { ('+'|'-') parse_muldiv }
parse_muldiv → parse_pow   { ('*'|'/'|'* *') parse_pow }
parse_pow    → parse_unary { '^' parse_pow }  /* 右结合 */
parse_unary  → ('+'|'-') parse_unary | parse_primary
parse_primary → NUM | VAR | '(' expr ')' | CMD args
```

- [ ] **Step 2: LaTeX 命令解析**

`\frac{a}{b}` → `EC_DIV`。`\sqrt{x}` → `EC_SQRT`。`\sin x` → `EC_SIN(x)`。`\pi` → `EC_PI`。`\int f dx` → `EC_INT`。`\lim_{x->a} f` → `EC_LIMIT`。`\sum_{i=a}^{b}` → `EC_SUM`。

- [ ] **Step 3: 验证**

```c
Expr *e = ec_parse("x^2 + 3*x + 1");
ec_print(e);  /* 树形输出 */
```
Expected: 正确解析为 AST

---

### Task 5: 统一求值引擎

**Files:**
- Modify: `Eculidim/src/eculid_eval.c/h`

- [ ] **Step 1: 实现 ECEnv**

`ec_env_new/free/bind/bind_many/lookup`。

- [ ] **Step 2: 实现初等函数数值版**

所有 `ec_*_d` 函数使用 `<math.h>` 实现（70+ 个）。

- [ ] **Step 3: 实现 ECValue 求值**

`ec_value_is_real/as_real/as_complex/free`。`ec_eval_sym/numeric/partial/const`。

- [ ] **Step 4: 实现代入**

`ec_substitute`, `ec_substitute_val`, `ec_substitute_func`。

- [ ] **Step 5: 实现复数运算**

所有 `ec_c_*` 函数（加/减/乘/除/幂/根/三角/双曲/极坐标转换）。

- [ ] **Step 6: 实现矩阵运算**

`ec_mat_alloc/clone/get/det/inverse/mul/solve/lu/lu_solve/cond/rank/free`。

- [ ] **Step 7: 验证**

```c
ECEnv env; ec_env_init(&env);
ec_env_bind(&env, "x", 2.0);
ECValue v = ec_eval_num(ec_parse("x^2 + 1"), &env);
assert(ec_value_as_real(&v) == 5.0);
```

---

### Task 6: 全局符号表

**Files:**
- Modify: `Eculidim/src/eculid_symtab.c/h`

- [ ] **Step 1: 实现 ECSymTab**

`ec_symtab_init/free`。`ec_symtab_define`（函数定义）。`ec_symtab_set_var`（变量赋值）。`ec_symtab_lookup`/`ec_symtab_lookup_var`。`ec_symtab_list/remove/clear`。

- [ ] **Step 2: 注册内置常量**

`ec_symtab_init_builtins` 注册 `pi`, `e`, `i`, `inf`。

- [ ] **Step 3: 验证**

```c
ECSymTab tab; ec_symtab_init(&tab);
ec_symtab_define(&tab, "f", "x", "sin(x) + x^2");
Expr *f = ec_symtab_lookup(&tab, "f");
assert(f != NULL);
```

---

### Task 7: 化简与变换

**Files:**
- Modify: `Eculidim/src/eculid_simplify.c/h`

- [ ] **Step 1: 常量折叠规则**

`0+x→x`, `1·x→x`, `x⁰→1`, `0·x→0`, `x/1→x`, `x-x→0`。

- [ ] **Step 2: 代数化简**

合并同类项 `x+x→2x`。幂运算化简 `x¹→x`, `x·x→x²`。分式化简。

- [ ] **Step 3: 三角恒等化简**

`sin²x+cos²x→1`, `ln(eˣ)→x`, `e^(ln x)→x`。

- [ ] **Step 4: 多项式除法**

`ec_polynomial_div`。

- [ ] **Step 5: 验证**

```c
Expr *r = ec_simplify(ec_parse("x + x + x"));
/* r = 3x */
```

---

### Task 8: 符号求导

**Files:**
- Modify: `Eculidim/src/eculid_diff.c/h`

- [ ] **Step 1: 基本求导规则**

常数→0、变量→1、加法→和导、乘法→积导、除法→商导、幂→链式。

- [ ] **Step 2: 初等函数求导**

`sin→cos`, `cos→-sin`, `tan→sec²`, `eˣ→eˣ`, `ln→1/x`, `√→1/(2√)`。

- [ ] **Step 3: 高阶导与偏导**

`ec_diff_n`, `ec_partial_diff`, `ec_total_diff`。

- [ ] **Step 4: 推导步骤记录**

`ec_diff_with_steps`，每步记录 LaTeX 和规则名。

- [ ] **Step 5: 验证**

```c
Expr *r = ec_diff(ec_parse("x^2 + 3*x + 1"), 'x');
char *latex = ec_to_latex(r);
assert(strstr(latex, "2") && strstr(latex, "x"));
```

---

### Task 9: 符号积分

**Files:**
- Modify: `Eculidim/src/eculid_integrate.c/h`

- [ ] **Step 1: 基本积分规则**

幂函数、三角、指数的基本积分表。

- [ ] **Step 2: 定积分**

`ec_definite_integral`：先求不定积分，再代入上下限。

- [ ] **Step 3: 无解析解检测**

`ec_has_integral` 返回0时，`ec_numerical_integral` 用 Simpson 法则。

- [ ] **Step 4: 验证**

```c
Expr *r = ec_integrate(ec_parse("x^2"), 'x');
/* r = x^3/3 */
```

---

### Task 10: 方程求解

**Files:**
- Modify: `Eculidim/src/eculid_solve.c/h`

- [ ] **Step 1: 线性方程求解**

`ec_solve_linear`。

- [ ] **Step 2: 二次方程**

`ec_solve_quadratic`，判别式处理。

- [ ] **Step 3: 数值求根**

`ec_solve_numeric_ex`：Newton、二分、Secant、Brentq 四种方法。

- [ ] **Step 4: 验证**

```c
EculidRoots r = ec_solve(ec_parse("x^2 - 4 = 0"), "x");
assert(r.count == 2);
```

---

### Task 11: 极限与级数

**Files:**
- Modify: `Eculidim/src/eculid_limit.c/h`
- Modify: `Eculidim/src/eculid_series.c/h`
- Modify: `Eculidim/src/eculid_sumprod.c/h`

- [ ] **Step 1: 极限**

直接代入法。未定式化简（0/0, ∞/∞ 型）。左/右极限。

- [ ] **Step 2: Taylor/Maclaurin 展开**

循环求导并代入。`ec_series_revert`。

- [ ] **Step 3: 求和闭合形式**

内置常见求和表：`Σk`, `Σk²`, `Σk³`。

---

### Task 12: LaTeX 输出与推导过程

**Files:**
- Modify: `Eculidim/src/eculid_latex.c/h`

- [ ] **Step 1: AST → LaTeX**

遍历 AST 节点，生成 LaTeX 字符串。`ec_to_latex_pretty` 美化格式。`ec_to_latex_tree` ASCII 树。

- [ ] **Step 2: 推导步骤渲染**

`ec_derivation_render`（纯文本）和 `ec_derivation_render_latex`（LaTeX align 环境）。

- [ ] **Step 3: LaTeX 命令表**

`ec_latex_cmd_table`：约100个 LaTeX 命令到节点类型的映射。

- [ ] **Step 4: JSON 输出引擎**

新建 `src/eculid_json.c/h`，实现：

```c
typedef struct {
    char *result;
    char **steps;
    int n_steps;
    ECError error;
} ECJSONOutput;

ECJSONOutput* ec_json_new(void);
void ec_json_set_result(ECJSONOutput *o, const char *latex);
void ec_json_add_step(ECJSONOutput *o, const char *latex, const char *rule);
void ec_json_set_error(ECJSONOutput *o, ECErrorCode code, const char *msg);
char* ec_json_render(const ECJSONOutput *o);
void ec_json_free(ECJSONOutput *o);
```

---

### Task 13: 会话管理与状态持久化

**Files:**
- Modify: `Eculidim/src/eculid_session.c/h`

- [ ] **Step 1: EculidSession 扩展**

`json_mode` 字段，`steps_enabled` 字段，`saved_count`（已保存记录数）。

- [ ] **Step 2: ans 管理**

`ec_session_get_ans`/`ec_session_set_ans`。

- [ ] **Step 3: 历史记录**

`ec_session_add`（追加历史，超256条丢弃最旧），`ec_session_history`。

- [ ] **Step 4: 状态持久化（核心新增）**

```c
/* 内存 AST → 文件 LaTeX 序列化 */
int ec_session_save(ECSymTab *tab, const char *filepath);
/*
 * 写入最近 5 次会话的 LaTeX 格式：
 * # Eculidim Session State
 * # === Entry 1 ===
 * def x = 3
 * ans = x^2 + 1
 * # === Entry 2 ===
 * def f(x) = sin(x)
 * ...
 */

/* 文件 LaTeX → 内存 AST 反序列化 */
int ec_session_load(ECSymTab *tab, const char *filepath);
```

`ec_session_free` 时自动调用 `ec_session_save`。

- [ ] **Step 5: 验证**

```c
EculidSession *s = ec_session_new();
ec_session_set_ans(s, ec_parse("x^2 + 3*x"));
Expr *ans = ec_session_get_ans(s);
char *latex = ec_to_latex(ans);
assert(strstr(latex, "x"));
ec_session_save(ec_session_symtab(s), "session.ec");
/* 重启后 */
ECSymTab tab; ec_symtab_init(&tab);
ec_session_load(&tab, "session.ec");
```

---

### Task 14: REPL 总控（CLI 核心）

**Files:**
- Modify: `Eculidim/src/eculid_main.c`

- [ ] **Step 1: 命令行参数解析**

```c
int main(int argc, char *argv[]) {
    if (argc > 1) {
        if (strcmp(argv[1], "--json") == 0) {
            /* Agent 模式：自动 JSON 输出 */
            return ec_cli_json(argc, argv);
        }
        /* 单次计算模式 */
        return ec_cli_once(argc, argv);
    }
    return ec_repl();  /* 交互模式 */
}
```

- [ ] **Step 2: 主 REPL 循环**

提示符 `ec>`，读取一行，检测 `\json`/`\text` 切换输出模式，检测模式切换命令（`\diff` 等），其余作为 LaTeX 表达式解析求值输出。

- [ ] **Step 3: JSON 输出分支**

当 `s->json_mode == 1` 时，`ECJSONOutput` 渲染后输出，末尾不带 `---END---`。

当 `s->json_mode == 0` 时，直接输出 LaTeX，末尾附 `---END---\n`。

- [ ] **Step 4: 子模式循环**

每个模式一个函数：
```c
int ec_mode_diff(EculidSession *s, char *input);
int ec_mode_integrate(EculidSession *s, char *input);
int ec_mode_solve(EculidSession *s, char *input);
int ec_mode_numeric(EculidSession *s, char *input);
int ec_mode_series(EculidSession *s, char *input);
int ec_mode_sum(EculidSession *s, char *input);
int ec_mode_limit(EculidSession *s, char *input);
```

- [ ] **Step 5: define 模式**

识别 `f(x) = expr` 语法，调用 `ec_symtab_define`。识别 `\ans`，替换为最近结果。

- [ ] **Step 6: numeric 模式批量代入**

识别 `{x=v1, y=v2, ...}` 语法，批量绑定 `ec_env_bind_many`。

- [ ] **Step 7: 错误处理**

所有错误走 `ECError` 结构，JSON 模式写入 `error` 字段，文本模式输出 `Error EC###: message`。

- [ ] **Step 8: help 输出（英文）**

```
Eculidim v1.0 — Mathematical Tool for AI Agents

Usage:
  eculidim              Start interactive REPL
  eculidim --json      Start REPL with JSON output (default for agents)

Modes (enter with \cmd):
  \diff       Symbolic differentiation
  \int        Symbolic integration (definite/indefinite)
  \solve      Equation solving
  \series     Taylor/Maclaurin expansion
  \sum        Summation (closed forms)
  \limit      Limit evaluation
  \num        Numerical evaluation
  \normal     Return to normal mode

Commands:
  \def f(x)=...  Define global function
  \ans          Show last result
  \hist         Show history (last 10)
  \steps on|off  Toggle derivation steps
  \json         Switch to JSON output
  \text         Switch to text output
  \help         Show this help
  exit         Exit (auto-saves last 5 entries)

Error codes: see docs/error-codes.md
```

---

### Task 15: 端到端验收测试

**Files:**
- Modify: `Eculidim/src/test.c`
- Create: `Eculidim/test_repl.sh`

- [ ] **Step 1: 单元测试扩展**

所有 Task 2-13 的函数测试追加到 `test.c`。

- [ ] **Step 2: CLI 集成测试**

```bash
# 文本模式
echo -e "diff x^2\nint x^2\nnum {x=3} x^2+1\nexit" | ./Eculidim
# Expected: 2x, x^3/3+C, 10

# JSON 模式
echo -e "diff x^2\nexit" | ./Eculidim --json
# Expected: valid JSON with result, steps, error fields
```

- [ ] **Step 3: 编译并运行**

Run: `gcc -DTEST -std=c99 src/*.c -o test -lm && ./test`

Expected: 所有断言通过

---

## 自审检查

**Spec 覆盖：**
- [x] 所有数学初等函数（AST 节点 + 求值）
- [x] 符号求导（含步骤）
- [x] 符号积分（含数值备选）
- [x] 方程求解（线性/二次/数值）
- [x] 极限（左右极限）
- [x] Taylor/Maclaurin 展开
- [x] 求和（闭合形式）
- [x] 全局符号表（函数/变量共享）
- [x] 多模式 CLI（8个模式）
- [x] ans 全局共享
- [x] 批量代入 `{}`
- [x] JSON/文本双输出格式
- [x] `--json` 自动进入 JSON 模式
- [x] 状态持久化（内存 AST + 文件 LaTeX，exit 保存最近 5 次）
- [x] 英文错误码（EC001-EC012）+ 错误码指南文档
- [x] 历史记录
- [x] 无外部依赖（仅 math.h）
- [x] 提示符 `ec>` 无 `[]`
- [x] 错误信息全英文
- [x] Agent 通过 stdin/stdout 直接调用

**占位符扫描：** 无 TODO/TBD
**类型一致性：** `Expr*` / `ECValue` / `ECEnv*` / `ECSymTab*` / `ECJSONOutput*` 全程一致
