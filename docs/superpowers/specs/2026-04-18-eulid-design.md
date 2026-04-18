# Eculidim 设计规格

## 1. 项目概述

**Eculidim** — 面向 AI Agent 的 C 语言数学运算工具。AI Agent 和人共用同一套 REPL，通过 LaTeX 输入、LaTeX 输出，支持状态持久化、推导步骤和全局符号表。模块化可插拔架构。

## 2. 核心模块

### 2.1 符号引擎（Symbolic Engine）
- **求导** `d/dx expr` — 一阶/高阶导数、多变量偏导
- **积分** `int expr dx` — 不定积分与定积分
- **化简** `simplify expr` — 代数化简、合并同类项
- **因式分解** `factor expr`
- **方程求解** `solve eq = 0` — 代数方程求根
- **极限** `lim x->0 expr`
- **级数展开** `series expr at x=0` — Taylor/Maclaurin

### 2.2 数值引擎（Numeric Engine）
- 基础算术（+ − × ÷ ^）
- 矩阵运算（求逆、行列式、特征值近似）
- 所有初等函数（见 §5）
- 支持复数运算（含 `i`）

### 2.3 验证器（Validator）
- LaTeX 语法合法性检查
- 量纲一致性检查
- 表达式维度检查

## 3. 架构

```
Eculidim/
├── include/
│   └── eculid.h          # 统一头文件，导出所有公共 API
├── src/
│   ├── eculid_main.c     # 主入口 + REPL 总控
│   ├── eculid_ast.c/h    # AST 节点定义
│   ├── eculid_lexer.c/h  # LaTeX 词法分析
│   ├── eculid_parser.c/h # 递归下降解析
│   ├── eculid_simplify.c/h # AST 化简
│   ├── eculid_diff.c/h   # 符号求导
│   ├── eculid_integrate.c/h # 符号积分
│   ├── eculid_solve.c/h  # 方程求解
│   ├── eculid_limit.c/h  # 极限
│   ├── eculid_series.c/h # 级数展开
│   ├── eculid_sumprod.c/h # 求和与连乘
│   ├── eculid_eval.c/h  # 统一求值引擎
│   ├── eculid_latex.c/h  # LaTeX 解析与输出
│   ├── eculid_validator.c/h # 量纲/维度检查
│   ├── eculid_plugin.c/h  # 插件注册与调度
│   ├── eculid_session.c/h # 会话管理（历史 + ans + 链式计算）
│   ├── eculid_symtab.c/h  # 全局符号表
│   └── eculid_util.c/h   # 工具函数
├── Makefile
└── README.md
```

**无外部依赖**，纯标准 C（`-std=c99`）。

## 4. CLI 交互设计

### 4.1 运行方式

- 双击 `eculidim.exe` 或在 PowerShell 输入 `eculidim` → 直接进入 REPL
- `eculidim --json` → 自动进入 JSON 输出模式（**Agent 默认**）
- `exit` 退出，**自动保存最近 5 次会话记录**到工作文件

### 4.2 交互对象

- **同一套 REPL**，人和 Agent 共用，交互方式完全相同
- Agent 通过 **stdin/stdout** 操控 REPL 进程：启动 → 写命令 → 读结果 → 继续 → `exit`

### 4.3 提示符

无 `[]`，直接显示当前模式：

```
ec>          ← 普通/求值模式（默认）
diff>        ← 求导模式
int>         ← 积分模式
solve>       ← 求解模式
series>      ← 级数展开模式
sum>         ← 求和模式
limit>       ← 极限模式
num>         ← 数值计算模式
```

进入方式：输入 `\diff`、`\int`、`\solve` 等命令切换。返回主模式：输入 `\normal` 或 `\eval`。

## 5. 状态管理

### 5.1 内存层

- REPL 内部维护状态（`ans`、变量、函数），**跨命令保持**
- 内存中存 **AST 节点**（求值快，结构化）

### 5.2 文件层

- 文件中存 **LaTeX 字符串**（人可读、可直接编辑）
- `exit` 时：AST → 序列化 LaTeX → 写入工作文件（最近 5 次）
- 启动时：从工作文件加载 LaTeX → 解析成 AST → 恢复状态

### 5.3 ans 变量

最近一次计算结果存储在 `ans`，全局共享，任何模式下可用。

```
ec> 3 + 5
8
ec> ans * 2
16
```

## 6. 输出格式

### 6.1 JSON 模式（Agent 默认）

`eculidim --json` 或运行时输入 `\json` 进入。输出为完整 JSON 对象：

```json
{
  "result": "\\frac{d}{dx}x^2 = 2x",
  "steps": [
    "\\frac{d}{dx}x^2",
    "2x \\quad \\text{Power Rule}"
  ],
  "error": null
}
```

JSON 定界明确，Agent 解析即知结构，无需额外定界。

### 6.2 文本模式（人读）

默认纯 LaTeX 输出，`\text` 切换。末尾附 `---END---` 分隔：

```
ec> diff x^2
2x
---END---
```

### 6.3 推导步骤

`\steps` 命令追加分步 LaTeX：

```
ec> \steps on
ec> diff x^2
\\frac{d}{dx}x^2
= 2x      [Power Rule]
```

两种都提供，主输出 LaTeX，`\steps` 命令单独输出推导步骤。

## 7. 命令参考

| 命令 | 功能 |
|------|------|
| `\diff` | 进入求导模式 |
| `\int` | 进入积分模式 |
| `\solve` | 进入求解模式 |
| `\series` | 进入级数模式 |
| `\sum` | 进入求和模式 |
| `\limit` | 进入极限模式 |
| `\num` | 进入数值模式 |
| `\normal` / `\eval` | 返回普通模式 |
| `\def f(x)=...` | 定义全局函数/变量 |
| `\ans` | 输出最近结果 |
| `\hist` | 历史记录 |
| `\steps on\|off` | 推导步骤开关 |
| `\json` | 切换 JSON 模式 |
| `\text` | 切换文本模式 |
| `\help` | 显示帮助 |
| `exit` | 退出（自动保存 5 次记录）|

## 8. numeric 模式批量代入

用 `{}` 一次性绑定多变量：

```
num> {x=2, y=3, z=5}
num> x*y + z
11
num> {x=1, y=2, z=3}
num> x*y + z
7
```

## 9. 错误处理

### 9.1 输出格式

所有输出均为英文，JSON 模式中 `error` 字段携带错误信息：

```json
{
  "result": null,
  "steps": null,
  "error": {
    "code": "EC001",
    "message": "Parse error at position 5: unexpected token ')'"
  }
}
```

### 9.2 错误码定义

详见 `docs/error-codes.md`（错误码指南，供 Agent 查阅）：

| 错误码 | 含义 |
|--------|------|
| EC001 | LaTeX 解析错误 |
| EC002 | 未定义的变量 |
| EC003 | 不支持的 LaTeX 命令 |
| EC004 | 除零错误 |
| EC005 | 积分无解析解（自动切换数值积分）|
| EC006 | 方程无解 |
| EC007 | 定义语法错误（函数/变量定义）|
| EC008 | 模式切换错误 |
| EC009 | 文件 I/O 错误（状态保存/恢复）|
| EC010 | 推导步骤生成失败 |
| EC011 | 级数展开收敛失败 |
| EC012 | 无效的命令参数 |

## 10. 初等函数支持

**算术**：`+ − × ÷ ^ √ ⁿ√ mod \bmod`

**指对数**：`\exp`, `\ln`, `\log`, `\log_{b}`

**三角**：`\sin \cos \tan \cot \sec \csc`
**反三角**：`\arcsin \arccos \arctan`
**双曲**：`\sinh \cosh \tanh`

**其他**：`\floor \rfloor \lceil \rceil \sign \sgn \max \min`

**微积分**：`d/dx \int \lim \partial \sum \prod`

**常量**：`i e \pi \infty`

## 11. 插件式注册机制（Plugin Registry）

每个操作（求导、积分等）作为独立插件注册，通过统一接口调用：

```c
typedef struct {
    const char *name;        // "diff", "integrate", "solve"...
    const char *latex_cmd;   // 注册的 LaTeX 命令或前缀
    Expr* (*apply)(Expr *input, void *ctx);
    const char *description;
} EculidPlugin;

void eculid_register_plugin(const EculidPlugin *plugin);
Expr* eculid_dispatch(const char *op, Expr *input, void *ctx);
```

## 12. 构建

```makefile
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2 -I./include
OUT     = Eculidim
SRCS    = $(wildcard src/*.c)
OBJS    = $(SRCS:.c=.o)

all: $(OUT)
$(OUT): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OUT) -lm
src/%.o: src/%.c include/eculid.h
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OUT) src/*.o
.PHONY: all clean
```

## 13. 验收标准

1. `make` 成功编译，生成 `Eculidim.exe`，无警告
2. `./Eculidim --json` 输出 JSON 格式，`./Eculidim` 输出纯文本
3. `diff> x^2` 输出 `2x`（LaTeX）
4. `int> x^2` 输出 `\frac{x^3}{3} + C`
5. `solve> x^2 - 4 = 0` 输出 `x = 2 \lor x = -2`
6. `num> {x=3} x^2 + 1` 输出 `10`
7. `ans` 跨命令有效，`\hist` 显示历史
8. `exit` 自动保存状态到文件，启动时恢复
9. 错误信息全英文，带错误码
10. 零外部依赖（仅 math.h）
