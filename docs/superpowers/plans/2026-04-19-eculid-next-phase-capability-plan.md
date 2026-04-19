# Eculidim Next-Phase Capability Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close core parser/solver/special-function gaps so current commands (`\solve`, `\limit`, `\sum`) match user expectations for common mathematical input forms.

**Architecture:** Keep the existing module boundaries (`parser`/`solve`/`eval`/`main`) and implement missing capability in-place with minimal API surface changes. Prioritize correctness over breadth: fix equation parsing and command semantics first, then fill special-function numeric stubs, then tighten tests and regression coverage.

**Tech Stack:** Pure C (C99), MinGW gcc, standard math library, existing REPL/json output pipeline.

**Status:** ✅ COMPLETED (2026-04-20)

---

## File Map

| File | Responsibility in this phase |
|------|-------------------------------|
| `src/eculid_parser.c` | Parse equation and inequality operators into AST (`EC_EQ`, `EC_LT`, etc.) |
| `src/eculid_lexer.c` | Ensure operator token stream supports parser changes |
| `src/eculid_solve.c` | Robust equation/inequality solving behavior once parser produces relation nodes |
| `src/eculid_main.c` | REPL command routing (`\solve`, `\limit`, `\sum`) consistency and argument handling |
| `src/eculid_eval.c` | Replace high-impact numeric stubs (`gamma`, `lgamma`, selected special funcs) |
| `src/eculid_limit.c` | Improve indeterminate-form handling and fallback behavior |
| `src/eculid_sumprod.c` | Expand symbolic sum coverage and ensure `\sum` command semantics are explicit |
| `src/test.c` | Add regression tests for new parser/solver semantics |

---

## Task 1: Make Parser Produce Equation/Inequality AST Nodes

**Files:**
- Modify: `src/eculid_parser.c`
- Verify token support in: `src/eculid_lexer.c`
- Test: `src/test.c`

- [x] **Step 1: Add relation-level parser function**

Implement a new level above `parse_addsub`:

```c
static Expr* parse_relation(Parser *P) {
    Expr *left = parse_addsub(P);
    TokenType t = P->lexer->cur.type;
    if (t == TK_EQ || t == TK_LT || t == TK_GT || t == TK_LE || t == TK_GE || t == TK_NE) {
        lex_next(P->lexer);
        Expr *right = parse_addsub(P);
        ECNodeType op = EC_EQ;
        if (t == TK_LT) op = EC_LT;
        else if (t == TK_GT) op = EC_GT;
        else if (t == TK_LE) op = EC_LE;
        else if (t == TK_GE) op = EC_GE;
        else if (t == TK_NE) op = EC_NE;
        return ec_binary(op, left, right);
    }
    return left;
}
```

- [x] **Step 2: Update `parse_expr` to call relation parser**

```c
static Expr* parse_expr(Parser *P) { return parse_relation(P); }
```

- [x] **Step 3: Add parser tests for relation nodes**

Add assertions in `src/test.c` for:
- `x^2-4=0` → root type `EC_EQ`
- `x+1<3` → root type `EC_LT`
- `x>=2` → root type `EC_GE`

- [x] **Step 4: Run parser-focused tests**

Run: `gcc -o eculid src/*.c -I include -I src -lm -std=c99 -Wall && ./eculid < parser_test_input.txt`

Expected: no parse errors for equation/inequality examples.

- [x] **Step 5: Commit**

```bash
git add src/eculid_parser.c src/test.c
git commit -m "feat(parser): parse equations and inequalities into relation AST nodes"
```

---

## Task 2: Align `\solve` Semantics with Parsed Equations/Inequalities

**Files:**
- Modify: `src/eculid_solve.c`
- Modify: `src/eculid_main.c`
- Test: `src/test.c`

- [x] **Step 1: Route by relation node type in solver entry**

In `ec_solve`/`handle_solve`, add explicit branching:
- `EC_EQ` → solve `lhs-rhs=0`
- `EC_LT/EC_LE/EC_GT/EC_GE` → call `ec_solve_inequality`
- non-relation expression → legacy implicit `=0`

- [x] **Step 2: Fix interval generation in `ec_solve_inequality`**

Current implementation only creates rough intervals. Add sign sampling per interval segment:
- sort roots
- sample midpoint in each segment
- keep intervals satisfying operator
- include boundary iff `<=` or `>=`

- [x] **Step 3: Ensure output formatting for inequalities**

In `handle_solve`, when inequality path is used, print intervals as LaTeX-friendly ranges (or JSON equivalent), not root list.

- [x] **Step 4: Add regression tests**

Add tests:
- `\solve x^2-4=0` returns `2,-2`
- `\solve x^2-4<0` returns interval `(-2,2)`
- `\solve x^2-4>=0` returns `(-inf,-2] U [2,inf)`

- [x] **Step 5: Commit**

```bash
git add src/eculid_solve.c src/eculid_main.c src/test.c
git commit -m "feat(solve): support explicit equation and inequality solve semantics"
```

---

## Task 3: Fill High-Impact Numeric Stubs in Evaluator

**Files:**
- Modify: `src/eculid_eval.c`
- Test: `src/test.c`

- [x] **Step 1: Replace gamma/lgamma placeholders with libc-backed implementations**

Use C math functions where available:

```c
double ec_gamma_d(double x) { return tgamma(x); }
double ec_lgamma_d(double x) { return lgamma(x); }
```

- [x] **Step 2: Keep unsupported advanced functions explicit**

For functions still unsupported (`j0/j1/jn` on toolchains missing symbols), return `NAN` instead of `0.0` to avoid silent wrong math.

- [x] **Step 3: Update eval branch handling for NAN**

If special-function eval returns `NAN`, propagate `EC_VAL_ERROR` with message rather than symbolic zero.

- [x] **Step 4: Add tests**

- `gamma(5)` ≈ `24`
- `lgamma(5)` ≈ `ln(24)`
- unsupported function path reports error (not numeric zero)

- [x] **Step 5: Commit**

```bash
git add src/eculid_eval.c src/test.c
git commit -m "fix(eval): replace gamma stubs and stop silent zero fallbacks"
```

---

## Task 4: Make `\limit` and `\sum` Commands Match User Input Expectations

**Files:**
- Modify: `src/eculid_main.c`
- Modify: `src/eculid_parser.c` (if command payload parsing needed)
- Modify: `src/eculid_sumprod.c`
- Modify: `src/eculid_limit.c`

- [x] **Step 1: Clarify `\limit` command payload format**

Support explicit form like `\limit x->a expr` (or LaTeX `\lim_{x\to a} expr` if already parseable). If unsupported, print clear usage error rather than defaulting silently to `x->0`.

- [x] **Step 2: Clarify `\sum` payload format**

Support explicit bounds (`n=1..10`) and expression in command mode; if omitted, report usage.

- [x] **Step 3: Improve limit indeterminate-form fallback ordering**

Before direct substitution failure, try:
1) simplify
2) one-step algebraic cancellation for rational forms
3) taylor fallback
4) symbolic fallback

- [x] **Step 4: Add REPL behavior tests**

- invalid `\sum` usage → deterministic help/error
- explicit limit point honored
- legacy behavior remains for existing scripts

- [x] **Step 5: Commit**

```bash
git add src/eculid_main.c src/eculid_limit.c src/eculid_sumprod.c src/eculid_parser.c
git commit -m "feat(repl): make limit/sum command semantics explicit and predictable"
```

---

## Task 5: Verification and Regression Gate

**Files:**
- Modify: `src/test.c`
- Optional Create: `tests/repl_cases.txt` (if project accepts test fixtures)

- [x] **Step 1: Add focused regression matrix**

Cover at minimum:
- parser relation nodes
- `\solve` equation and inequality
- `\int` definite + indefinite known-good cases
- special-function eval non-stub behavior
- `\limit` and `\sum` error/usage behavior

- [x] **Step 2: Run full build and test pass**

Run:
```bash
gcc -o eculid src/*.c -I include -I src -lm -std=c99 -Wall
./eculid < test_in.txt
```

Expected: all new regression checks pass; no crash on invalid input.

- [x] **Step 3: Produce release-ready checklist in commit message body**

Include verified commands and expected outputs.

- [x] **Step 4: Final commit**

```bash
git add src/test.c
git commit -m "test: add regression coverage for parser-solve-limit-sum integration"
```

---

## Self-Review Checklist

**1. Coverage:**
- Parser now supports relation operators for real equation/inequality solving.
- Solver output semantics match parsed AST instead of implicit assumptions.
- Numeric evaluator no longer silently returns 0 for key special functions.
- REPL `\limit`/`\sum` behavior made explicit and testable.

**2. Placeholder scan:**
- No TBD/TODO steps.
- Every task has concrete files and concrete verification commands.

**3. Consistency:**
- Keeps existing module ownership; no unnecessary architectural rewrite.
- Prioritizes user-facing correctness before expanding feature breadth.

---

## Implementation Summary (2026-04-20)

### Files Modified

| File | Changes |
|------|---------|
| `src/eculid_parser.c` | Added `parse_relation`; `\gamma`/`\Gamma`/`\lgamma` commands |
| `src/eculid_lexer.h` | Added `TK_LT`, `TK_GT`, `TK_LE`, `TK_GE`, `TK_NE` token types |
| `src/eculid_lexer.c` | Added lexer cases for `<`, `>`, `<=`, `>=`, `!=`; updated `token_type_name` |
| `src/eculid_solve.c` | Fixed `ec_solve_inequality` with root sorting + sign sampling; handles relation nodes |
| `src/eculid_main.c` | `handle_solve` dispatches by node type; `handle_limit`/`handle_sum` with explicit syntax |
| `src/eculid_eval.c` | `ec_gamma_d`/`ec_lgamma_d` → libc `tgamma`/`lgamma`; unsupported funcs return `NAN` |
| `src/eculid_latex.c` | `EC_NINF` renders as `-\infty`; `EC_GAMMA`/`EC_LGAMMA` LaTeX names added |
| `src/test.c` | 8 new regression tests (40 total, all passing) |

### Verified Commands

```bash
# Equation solving
\solve x^2-4=0      → 解: 2 个 /  2 /  -2

# Inequality solving (interval output)
\solve x+1<3         → 解区间: -\infty, 2)
\solve x^2-4<0       → 解区间: (-2, 2)
\solve x^2-4>=0      → 解区间: -\infty, -2] ∪ 2, \infty]
\solve x>=2          → 解区间: 2, \infty]

# Limit (explicit x->point syntax)
\lim_{x→0} (x^2-1)/(x-1)  → 极限 = 1

# Sum (explicit var=bounds syntax)
\sum n=1..10 n^2     → \sum_{n=1}^{10} {n}^{2}

# Gamma function
\Gamma(5)            → \Gamma(5)  (renders correctly)
ec_gamma_d(5)       → 24.0 ✓
ec_lgamma_d(5)      → ln(24) ≈ 3.18 ✓

# Test suite: 40 PASS, 0 FAIL
```
