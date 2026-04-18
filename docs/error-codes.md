# Eculidim 错误码指南

> 本文档供 AI Agent 查阅。Eculidim 所有错误均通过英文 `error` 字段返回，JSON 和文本模式均适用。

## 输出格式

### JSON 模式

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

### 文本模式

```
Error EC001: Parse error at position 5: unexpected token ')'
```

**重要：** Agent 解析 JSON 响应时，**始终先检查 `error` 字段是否为 `null`**。非 `null` 表示发生错误。

---

## 错误码列表

| 错误码 | 常量 | 含义 | 典型示例 | Agent 建议 |
|--------|------|------|----------|------------|
| EC001 | `EC_ERR_PARSE` | LaTeX 解析错误 | `\frac{}{x}` 缺少分子 | 检查 LaTeX 语法，参照支持的命令列表重试 |
| EC002 | `EC_ERR_UNDEF_VAR` | 变量未定义 | 输入 `x + 1`，`x` 无定义 | 先行定义变量，如 `num> {x=2}` 或 `def x = 2` |
| EC003 | `EC_ERR_UNSUP_CMD` | 不支持的命令 | `\zeta(x)` 未支持 | 检查 `ec_supported_funcs()` 中列出的命令，使用支持的别名 |
| EC004 | `EC_ERR_DIV_ZERO` | 除零错误 | `1/0` | 检查表达式中的分母，确保不为零 |
| EC005 | `EC_ERR_NO_INT` | 积分无解析解 | `\int e^{x^2} dx` | Eculidim 会自动尝试数值积分，结果仍会返回（可能有精度损失）|
| EC006 | `EC_ERR_NO_SOLVE` | 方程无解（或无实解）| `x^2 + 1 = 0` 在实数范围 | 尝试复数模式，或使用 `num>` 模式求数值解 |
| EC007 | `EC_ERR_DEF_SYNTAX` | 定义语法错误 | `\def f() = x` 缺参数名 | 定义函数格式：`\def f(x) = x^2 + 1`，变量格式：`\def a = 3.14` |
| EC008 | `EC_ERR_MODE` | 模式内命令错误 | 在 `diff>` 下使用 `\num` | 切换回主模式 `ec>` 再使用，或在对应子模式下使用正确命令 |
| EC009 | `EC_ERR_IO` | 文件 I/O 错误 | 状态文件不可写、无权限 | 检查工作目录写权限，或指定可写路径 |
| EC010 | `EC_ERR_NO_STEPS` | 推导步骤不可用 | 当前表达式无法生成步骤 | 可继续使用，结果仍然正确，只是无分步推导 |
| EC011 | `EC_ERR_NO_CONV` | 级数收敛失败 | `\sum_{n=1}^{\\infty} 1/n` | 检查级数是否收敛，或限制展开阶数 |
| EC012 | `EC_ERR_BAD_ARG` | 参数错误 | `series x at x=inf order -1` | 检查参数范围：展开阶数须为正整数 |

---

## 支持的 LaTeX 命令

Eculidim 支持以下 LaTeX 命令（EC003 错误时对照）：

### 算术与幂
`+`, `-`, `*`, `/`, `^`, `\sqrt`, `\frac`, `\bmod`

### 三角函数
`\sin`, `\cos`, `\tan`, `\cot`, `\sec`, `\csc`

### 反三角函数
`\arcsin`, `\arccos`, `\arctan`

### 双曲函数
`\sinh`, `\cosh`, `\tanh`, `\coth`, `\sech`, `\csch`

### 指数与对数
`\exp`, `\ln`, `\log`, `\log10`, `\log2`

### 幂函数与根
`\sqrt`, `\cbrt`, `^`

### 取整函数
`\floor`, `\ceil`, `\round`, `\trunc`, `\abs`, `\sign`

### 初等常数
`\pi`, `\e`, `\i`, `\infty`

### 微积分
`\int`（不定积分）, `\int_{a}^{b}`（定积分）, `\frac{d}{dx}`（求导）, `\lim`, `\sum`, `\prod`

---

## JSON 响应结构

```json
{
  "result": "LaTeX 字符串，或 null（出错时）",
  "steps": ["分步 LaTeX 列表，或 null"],
  "error": {
    "code": "EC###",
    "message": "英文描述"
  }
}
```

**字段说明：**
- `result`: 成功时为 LaTeX 数学表达式字符串；出错时为 `null`
- `steps`: `steps on` 时为推导步骤列表；否则为 `null`
- `error`: `null` 表示无错误；非 `null` 表示出错，`code` 为错误码，`message` 为英文描述

---

## Agent 使用建议

1. **始终检查 error 字段** — 先判 `error == null`，再读取 `result`
2. **EC005（积分无解析解）** — Eculidim 会自动切换数值积分，结果仍返回
3. **EC006（方程无解）** — 可切换 `num>` 模式求数值近似解
4. **携带完整上下文** — 变量/函数定义尽量在同一会话中完成，避免 EC002
5. **状态持久化** — `exit` 时 Eculidim 自动保存最近 5 次，可跨会话恢复
6. **JSON 模式是默认推荐** — Agent 调用时使用 `eculidim --json`，确保语义精准传递
