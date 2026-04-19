---
name: eculidim-project-overview
description: Eculidim C math tool project structure, phases, and task history
type: project
originSessionId: 851ac0eb-2362-492e-be57-1dc912dcfdc0
---
# Eculidim 项目概览

## 项目信息
- **类型**: C 数学工具，面向 AI Agent
- **路径**: `C:/Users/yanwei/Desktop/Document_In_University/Codes/Eculidim/`
- **编译器**: MinGW gcc (`"C:\Program Files (x86)\Dev-Cpp\MinGW32\bin\gcc"`)
- **构建**: `make` 或直接 `gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm`
- **无外部依赖**：仅 math.h

## 项目结构
```
Eculidim/
├── include/eculid.h          # 统一头文件
├── src/
│   ├── eculid_main.c         # REPL + JSON 输出
│   ├── eculid_ast.c/h        # AST 节点
│   ├── eculid_lexer.c/h      # LaTeX 词法
│   ├── eculid_parser.c/h      # 递归下降解析器
│   ├── eculid_simplify.c/h   # 化简与变换
│   ├── eculid_diff.c/h        # 符号求导
│   ├── eculid_integrate.c/h   # 符号积分
│   ├── eculid_solve.c/h      # 方程求解
│   ├── eculid_limit.c/h      # 极限
│   ├── eculid_series.c/h      # Taylor/Maclaurin
│   ├── eculid_sumprod.c/h    # 求和与连乘
│   ├── eculid_eval.c/h       # 统一求值引擎
│   ├── eculid_latex.c/h      # LaTeX 输出 + Derivation 步骤
│   ├── eculid_validator.c/h   # 验证器
│   ├── eculid_plugin.c/h      # 插件注册
│   ├── eculid_session.c/h     # 会话管理 + 状态持久化
│   ├── eculid_symtab.c/h     # 全局符号表
│   ├── eculid_json.c/h        # JSON 输出引擎
│   ├── eculid_util.c/h        # 工具函数
│   └── test.c                 # 单元测试
├── docs/
│   └── error-codes.md         # 错误码指南
├── docs/
│   └── error-codes.md         # 错误码指南
├── docs/superpowers/
│   ├── plans/2026-04-18-eulid-implementation.md   # Tasks 1-15（已完成）
│   ├── plans/2026-04-19-eculid-feature-completion.md  # Tasks 16-23（已完成）
│   └── plans/2026-04-19-eculid-next-phase-capability-plan.md  # Tasks 1-5 阶段三（已完成）
└── Makefile
```

## 阶段划分

### 阶段一：初始实现（Tasks 1-15）✅ 已完成
**文档**: `docs/superpowers/plans/2026-04-18-eulid-implementation.md`

| Task | 内容 |
|------|------|
| 1 | 目录结构与编译验证（Makefile, 空壳文件）|
| 2 | AST 节点定义与基础操作（Expr, ECNodeType ~70种）|
| 3 | 词法分析器（LaTeX Tokenizer）|
| 4 | 递归下降解析器（优先级、\frac、\sqrt 等）|
| 5 | 统一求值引擎（ECEnv, 70+ 初等函数, 复数, 矩阵）|
| 6 | 全局符号表（函数/变量定义, 内置常量）|
| 7 | 化简与变换（常量折叠, 同类项合并, 三角恒等, 多项式除法）|
| 8 | 符号求导（含推导步骤 ec_diff_with_steps）|
| 9 | 符号积分（积分表, 数值积分备选）|
| 10 | 方程求解（线性/二次解析, Newton/二分/Secant/Brent 数值）|
| 11 | 极限与级数（Taylor/Maclaurin, 求和闭合形式）|
| 12 | LaTeX 输出与推导过程（ec_to_latex, ec_derivation_render）|
| 13 | 会话管理与状态持久化（内存 AST + 文件 LaTeX）|
| 14 | REPL 总控（多模式, JSON/text 双输出）|
| 15 | 端到端验收测试 |

### 阶段二：缺陷修复与功能补全（Tasks 16-23）✅ 已完成
**文档**: `docs/superpowers/plans/2026-04-19-eculid-feature-completion.md`

| Task | 内容 | 状态 |
|------|------|------|
| 16 | 修复解析器 `\frac{a}{b}`（支持表达式作为分子/分母）| ✅ |
| 17 | 支持 `{}` 分组语法（`\sin{x}` 等）| ✅ |
| 18 | 修复 Taylor/Maclaurin 返回 0（eval_point 修正）| ✅ |
| 19 | 修复 `1/x` 积分 bug + 补充积分表 + 双曲积分 | ✅ |
| 20 | 增强方程求解（多项式因式分解, 不等式求解）| ✅ |
| 21 | LaTeX 美化（移除冗余括号, `latex_needs_parens`）| ✅ |
| 22 | 定积分 REPL 语法 `\int_a^b` + 组合模式 `\int expr` | ✅ |
| 23 | 推导步骤接入 JSON steps 字段 | ✅ |

### 阶段三：能力强化（Next-Phase Tasks 1-5）✅ 已完成
**文档**: `docs/superpowers/plans/2026-04-19-eculid-next-phase-capability-plan.md`

| Task | 内容 | 状态 |
|------|------|------|
| 1 | 解析器关系节点（`parse_relation`，`TK_LT/GT/LE/GE/NE`）| ✅ |
| 2 | `\solve` 语义对齐（方程根 + 不等式区间）| ✅ |
| 3 | Gamma 函数桩替换（`tgamma`/`lgamma`，`\gamma`/`\Gamma` 解析）| ✅ |
| 4 | `\limit`/`\sum` 显式语法（`x->point`、`n=lo..hi`）| ✅ |
| 5 | 回归测试（40 个测试全部通过）| ✅ |

## 当前状态（阶段三已完成）
- **回归测试**: 40 PASS, 0 FAIL
- **支持的不等式**: `<`, `>`, `<=`, `>=`, `!=`
- **支持的特殊函数**: `\Gamma(n)` = tgamma, `\lgamma(x)` = lgamma
- **命令语法**: `\limit x->a expr`, `\sum n=m..k expr`

## REPL 使用方式
```
./Eculidim.exe                  # 交互模式
./Eculidim.exe --json "expr"   # 单次 JSON 模式
```
模式: `\diff`, `\int`, `\solve`, `\series`, `\sum`, `\limit`, `\num`
切换: `\json`, `\text`, `\steps on|off`
定义: `\def x = 3`, `\def f(x) = sin(x)`
特殊: `\ans`（上次结果）, `\hist`（历史）, `\help`

## 已知问题（阶段三结束后）
1. `\limit` 数值边界情况（1/x 在 0 处特殊处理）
2. `\Gamma(x)` 数值求值需要 `\num` 模式下的 parser 支持
3. REPL `\gamma` 等命令在组合模式下需要 `\Gamma(x)` 格式
