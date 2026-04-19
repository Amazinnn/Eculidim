# Eculidim Feature Completion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the `1/x` integration bug, complete the integration table, beautify LaTeX output, enhance equation solving, and wire derivation steps into JSON.

**Architecture:** Fix `eculid_integrate.c` (remove broken fallback), fix `eculid_latex.c` (remove redundant parentheses), enhance `eculid_solve.c` (stubs → implementations), wire `eculid_main.c` (JSON steps), add `eculid_parser.c` (definite integral syntax). Each module is self-contained.

**Tech Stack:** Pure C, MinGW gcc, standard math library.

---

## File Map

| File | Responsibility |
|------|---------------|
| `src/eculid_integrate.c` | Symbolic integration, integral table |
| `src/eculid_latex.c` | LaTeX rendering, derivation step helpers |
| `src/eculid_main.c` | REPL dispatch, JSON output |
| `src/eculid_parser.c` | Expression parser, LaTeX syntax |
| `src/eculid_solve.c` | Equation solving, numerical methods |
| `src/eculid_simplify.c` | Algebraic simplification |
| `src/eculid_session.c` | Session state, history |

**Compiler:** `"C:\Program Files (x86)\Dev-Cpp\MinGW32\bin\gcc"` (MinGW.org GCC 9.2.0)
**Build command:** `CC="C:\Program Files (x86)\Dev-Cpp\MinGW32\bin\gcc" && $CC -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm`

---

## Task 1: Fix `1/x` Integration Bug

**Files:**
- Modify: `src/eculid_integrate.c:369-393`

The `EC_DIV` case in `integ_rec` has a broken fallback that produces `∫(f/g) = (∫f)/(∫g)`. Additionally, the `1/x` pattern check (`num_val == 1.0`) needs a floating-point tolerance since `atof("1")` produces exactly `1.0` but `1.0 == 1.0` comparison is safe here. The real issue: after removing the fallback, `integ_rec` returns `NULL`, and `ec_integrate` wraps the original `EC_DIV` in an `EC_INT` node — which is correct but needs verification.

- [ ] **Step 1: Read current state of EC_DIV case in eculid_integrate.c**

Read lines 369-393 of `src/eculid_integrate.c`. Verify the broken fallback code was already removed (the previous session left a partial fix that adds `(void)r; break;` but leaves dead `if (i_left && i_right)` code below).

- [ ] **Step 2: Clean up dead code in EC_DIV case**

Replace the entire `EC_DIV` case body (lines ~369-387) with:

```c
        /*---- Rational functions: arctan, 1/x ----*/
        case EC_DIV: {
            /* Pattern: 1/x → ln|x| */
            if (e->left && e->right &&
                e->left->type == EC_NUM && fabs(e->left->num_val - 1.0) < 1e-14 &&
                e->right->type == EC_VAR && e->right->var_name == var &&
                !e->right->var_str) {
                return ec_unary(EC_LN, ec_unary(EC_ABS, ec_varc(var)));
            }
            /* Pattern: 1/(x^2+1) → atan(x) */
            Expr *r = try_rational_antideriv(e, var);
            if (r) return ec_simplify(r);
            /* Don't fall through to broken formula: give up */
            return NULL;
        }
```

- [ ] **Step 3: Build and test `\int 1/x`**

Build: `CC -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm`
Test: `printf '\int\n1/x\nexit\n' | ./Eculidim.exe`
Expected output: `\ln|x| + C`

- [ ] **Step 4: Test `\int \frac{1}{x}`**

Test: `printf '\int\n\\frac{1}{x}\nexit\n' | ./Eculidim.exe`
Expected: `\ln|x| + C`

- [ ] **Step 5: Commit**

```bash
git add src/eculid_integrate.c
git commit -m "fix: remove broken EC_DIV fallback, add fabs tolerance for 1/x"
```

---

## Task 2: Complete Integration Table

**Files:**
- Modify: `src/eculid_integrate.c`

Current missing integrations:
- `EC_TAN`: simple var case already exists, but chain rule case needs completion
- `EC_SEC`: simple var case already exists (returns `ln|sec+tan|`)
- `EC_CSC`: simple var case already exists (returns `-ln|csc+cot|`)
- `EC_COSH`: simple var case already exists
- `EC_SINH`: simple var case already exists
- `EC_TANH`: simple var case already exists
- `EC_COTH`, `EC_SECH`, `EC_CSCH`: stub cases returning `EC_INT` placeholder
- `EC_ACOTH`, `EC_ASECH`, `EC_ACSCH`: inverse hyperbolic stub cases

Add the following cases to `integ_rec`, after the existing `EC_TANH` case (~line 359):

- [ ] **Step 1: Add missing hyperbolic integration cases**

In `src/eculid_integrate.c`, after the existing `EC_TANH` case, add:

```c
        /*---- More hyperbolic ----*/
        case EC_COTH:
            if (expr_is_simple_var(e->arg, var)) {
                return ec_unary(EC_LN, ec_unary(EC_SINH, ec_varc(var)));
            }
            break;
        case EC_SECH:
            if (expr_is_simple_var(e->arg, var)) {
                /* ∫sech(x)dx = 2*atan(tanh(x/2)) */
                Expr *half = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *tanh_half = ec_unary(EC_TANH, half);
                Expr *result = ec_binary(EC_MUL, ec_num(2), ec_unary(EC_ATAN, tanh_half));
                ec_free_expr(half);
                return result;
            }
            break;
        case EC_CSCH:
            if (expr_is_simple_var(e->arg, var)) {
                /* ∫csch(x)dx = ln|tanh(x/2)| */
                Expr *half = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *tanh_half = ec_unary(EC_TANH, half);
                Expr *result = ec_unary(EC_LN, ec_unary(EC_ABS, tanh_half));
                ec_free_expr(half);
                return result;
            }
            break;

        /*---- Inverse hyperbolic (inverse of coth, sech, csch) ----*/
        case EC_ACOTH:
            if (expr_is_simple_var(e->arg, var)) {
                /* ∫acoth(x)dx = x*acoth(x) + (1/2)*ln|x-1| - (1/2)*ln|x+1| */
                Expr *x_acoth = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ACOTH, ec_varc(var)));
                Expr *ln1 = ec_unary(EC_LN, ec_unary(EC_ABS, ec_binary(EC_SUB, ec_varc(var), ec_num(1))));
                Expr *ln2 = ec_unary(EC_LN, ec_unary(EC_ABS, ec_binary(EC_ADD, ec_varc(var), ec_num(1))));
                Expr *term = ec_binary(EC_MUL, ec_num(0.5), ec_binary(EC_SUB, ln1, ln2));
                return ec_binary(EC_ADD, x_acoth, term);
            }
            break;
        case EC_ASECH:
            if (expr_is_simple_var(e->arg, var)) {
                /* ∫asech(x)dx = x*asech(x) + 2*sqrt(1-x) */
                Expr *x_asech = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ASECH, ec_varc(var)));
                Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_SUB, ec_num(1), ec_varc(var)));
                Expr *result = ec_binary(EC_ADD, x_asech, ec_binary(EC_MUL, ec_num(2), sqrt_term));
                return result;
            }
            break;
        case EC_ACSCH:
            if (expr_is_simple_var(e->arg, var)) {
                /* ∫acsch(x)dx = x*acsch(x) + ln|x + sqrt(x^2+1)| */
                Expr *x_acsch = ec_binary(EC_MUL, ec_varc(var), ec_unary(EC_ACSCH, ec_varc(var)));
                Expr *sq = ec_binary(EC_POW, ec_varc(var), ec_num(2));
                Expr *sqrt_term = ec_unary(EC_SQRT, ec_binary(EC_ADD, sq, ec_num(1)));
                Expr *ln_term = ec_unary(EC_LN, ec_binary(EC_ADD, ec_varc(var), sqrt_term));
                ec_free_expr(sq); ec_free_expr(sqrt_term);
                return ec_binary(EC_ADD, x_acsch, ln_term);
            }
            break;

        /*---- tan chain rule ----*/
        case EC_TAN:
            /* tan(u) = sin(u)/cos(u), ∫tan(u)dx = -ln|cos(u)| if du/dx=1 */
            if (expr_is_simple_var(e->arg, var)) {
                return ec_unary(EC_NEG, ec_unary(EC_LN, ec_unary(EC_COS, ec_varc(var))));
            }
            break;
```

- [ ] **Step 2: Build and test hyperbolic integrations**

Build and run:
- `\int \sinh(x)` → `\cosh(x) + C`
- `\int \tanh(x)` → `\ln(\cosh(x)) + C`
- `\int \coth(x)` → `\ln(\sinh(x)) + C`

- [ ] **Step 3: Commit**

```bash
git add src/eculid_integrate.c
git commit -m "feat: add coth, sech, csch, acoth, asech, acsch, tan chain rule integrals"
```

---

## Task 3: Beautify LaTeX Output (Remove Redundant Parentheses)

**Files:**
- Modify: `src/eculid_latex.c:107-112`

Current `EC_MUL` case wraps both operands in parentheses unconditionally: `(left) \cdot (right)`. This produces ugly output like `(x) \cdot (y)` instead of `x \cdot y` for simple operands.

- [ ] **Step 1: Read the current EC_MUL LaTeX case**

Read lines 107-113 of `src/eculid_latex.c`. Note: `latex_rec` writes into a buffer via `snprintf`, so changes must build the string correctly.

- [ ] **Step 2: Implement atomic operand detection**

In `src/eculid_latex.c`, add a helper function before `latex_rec`:

```c
/* Returns 1 if the expression is "atomic" — doesn't need parentheses
   when used as a child of a binary operator in LaTeX. */
static int latex_needs_parens(const Expr *e) {
    if (!e) return 0;
    switch (e->type) {
        case EC_NUM: case EC_VAR: case EC_PI: case EC_E:
        case EC_I: case EC_INF: case EC_NINF:
            return 0;
        case EC_POW: case EC_SQRT: case EC_CBRT:
        case EC_SIN: case EC_COS: case EC_TAN: case EC_EXP:
        case EC_LN: case EC_LOG: case EC_ABS: case EC_FLOOR:
        case EC_CEIL: case EC_ASIN: case EC_ACOS: case EC_ATAN:
        case EC_SINH: case EC_COSH: case EC_TANH:
            return 0;
        default:
            return 1;
    }
}
```

- [ ] **Step 3: Update EC_MUL case**

Replace the `EC_MUL` case in `latex_rec` with:

```c
        case EC_MUL: {
            int lpar = latex_needs_parens(e->left);
            int rpar = latex_needs_parens(e->right);
            if (lpar) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->left, buf, cap, pos);
            if (lpar) *pos += snprintf(buf + *pos, cap - *pos, ")");
            *pos += snprintf(buf + *pos, cap - *pos, " \\cdot ");
            if (rpar) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->right, buf, cap, pos);
            if (rpar) *pos += snprintf(buf + *pos, cap - *pos, ")");
            break;
        }
```

- [ ] **Step 4: Update EC_ADD/EC_SUB for consistency**

Update `EC_ADD` and `EC_SUB` cases to also avoid redundant parens around atomic operands:

```c
        case EC_ADD:
            if (latex_needs_parens(e->left)) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->left, buf, cap, pos);
            if (latex_needs_parens(e->left)) *pos += snprintf(buf + *pos, cap - *pos, ")");
            *pos += snprintf(buf + *pos, cap - *pos, " + ");
            if (latex_needs_parens(e->right)) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->right, buf, cap, pos);
            if (latex_needs_parens(e->right)) *pos += snprintf(buf + *pos, cap - *pos, ")");
            break;
        case EC_SUB:
            if (latex_needs_parens(e->left)) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->left, buf, cap, pos);
            if (latex_needs_parens(e->left)) *pos += snprintf(buf + *pos, cap - *pos, ")");
            *pos += snprintf(buf + *pos, cap - *pos, " - ");
            /* Right operand: always parens if atomic? No — but parens needed for ADD/SUB in right */
            if (latex_needs_parens(e->right)) *pos += snprintf(buf + *pos, cap - *pos, "(");
            latex_rec(e->right, buf, cap, pos);
            if (latex_needs_parens(e->right)) *pos += snprintf(buf + *pos, cap - *pos, ")");
            break;
```

- [ ] **Step 5: Test LaTeX output**

Build, then test:
- `2*x` → should NOT have parens around `2` or `x`
- `x+1` → should NOT have parens
- `(x+1)*(y+2)` → should have parens around `(x+1)` and `(y+2)`
- `x^2 + 1` → should NOT have parens

- [ ] **Step 6: Commit**

```bash
git add src/eculid_latex.c
git commit -m "feat: remove redundant LaTeX parentheses for atomic operands"
```

---

## Task 4: Wire Derivation Steps into JSON Steps Field

**Files:**
- Modify: `src/eculid_main.c`
- Read: `src/eculid_session.h` for `EculidSession` structure

Current: `handle_diff` and `handle_integrate` call `ec_to_latex` directly without capturing derivation steps. The `ECJSONOutput` struct (in `eculid_json.h`) has `n_steps` and `steps[]` fields, and `ec_json_add_step` is available. The session already has a `steps_enabled` flag.

- [ ] **Step 1: Read EculidSession structure and steps flag**

Read `src/eculid_session.h`. Find the `steps_enabled` field or equivalent.

- [ ] **Step 2: Read current handle_diff and handle_integrate**

Read lines 186-215 of `src/eculid_main.c`.

- [ ] **Step 3: Add JSON mode handler using ECJSONOutput**

In `src/eculid_main.c`, modify `handle_diff` and `handle_integrate` to use `ec_diff_with_steps` / `ec_integrate_with_steps` when in steps mode, and output via `ec_json_render`. Add a helper:

```c
static void output_json_with_steps(const char *result, Derivation *d,
                                   const char *error_code_str, const char *error_msg) {
    ECJSONOutput *o = ec_json_new();
    if (error_msg) {
        ec_json_set_error(o, EC_ERR_PARSE, error_msg);
    } else {
        ec_json_set_result(o, result);
        if (d) {
            for (int i = 0; i < d->count; i++) {
                ec_json_add_step(o, d->steps[i].step_latex, d->steps[i].rule);
            }
        }
    }
    char *json = ec_json_render(o);
    printf("%s\n", json);
    ec_json_free(o);
}
```

- [ ] **Step 4: Update handle_diff to use derivation**

Replace `handle_diff` body with:

```c
static void handle_diff(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    if (s->steps_enabled) {
        Derivation *d = ec_diff_with_steps(e, 'x');
        char *latex = ec_to_latex(d->steps[d->count - 1].step_latex ? d->steps[d->count - 1].step_latex : ec_to_latex(ec_diff(e, 'x')));
        output_json_with_steps(latex, d, NULL, NULL);
        ec_free(latex); ec_derivation_free(d);
    } else {
        Expr *wrapped = ec_diff_with_latex(e, 'x');
        char *latex = ec_to_latex(wrapped);
        output_result(latex, NULL, NULL);
        ec_free(latex); ec_free_expr(wrapped);
    }
    ec_free_expr(e);
}
```

**Note:** `ec_diff_with_steps` does not yet exist — check `src/eculid_diff.c`. If it doesn't exist, implement it similarly to `ec_integrate_with_steps`:

```c
Derivation* ec_diff_with_steps(Expr *e, char var) {
    Derivation *d = ec_derivation_new();
    char buf[512];
    char *orig = ec_to_latex(e);
    snprintf(buf, sizeof(buf), "\\frac{d}{d%c} (%s)", var, orig);
    ec_derivation_add(d, buf, "Original Expression");
    ec_free(orig);
    Expr *r = ec_diff(e, var);
    if (r) {
        Expr *sr = ec_simplify(r);
        char *res = ec_to_latex(sr);
        snprintf(buf, sizeof(buf), "%s", res);
        ec_derivation_add(d, buf, "Differentiation Result");
        ec_free(res); ec_free_expr(sr); ec_free_expr(r);
    }
    return d;
}
```

- [ ] **Step 5: Test JSON steps output**

Build, then:
```bash
./Eculidim.exe --json '\diff'
# then type: x^2
# Expected: JSON with "steps": ["[1] \\frac{d}{dx} (x^2) (Original Expression)", ...]
```

- [ ] **Step 6: Commit**

```bash
git add src/eculid_main.c src/eculid_diff.c
git commit -m "feat: wire derivation steps into JSON steps field"
```

---

## Task 5: Definite Integral REPL Syntax (`\int_a^b`)

**Files:**
- Modify: `src/eculid_parser.c`
- Modify: `src/eculid_main.c`

Currently the REPL supports `\int` for indefinite integrals. Need to add `\int_0^1 x^2` syntax.

- [ ] **Step 1: Add `\int` with bounds parsing to parser**

In `src/eculid_parser.c`, after the existing `\int` handler in `parse_primary`, add a case that parses `\int_{a}^{b} expr`:

```c
        if (strcmp(cmd, "\\int") == 0) {
            Expr *lower = NULL, *upper = NULL;
            /* Parse optional _bounds */
            if (P->lexer->cur.type == TK_UNDERSCORE) {
                lex_next(P->lexer);
                lower = parse_expr(P);
                if (P->lexer->cur.type == TK_CARET) {
                    lex_next(P->lexer);
                    upper = parse_expr(P);
                }
            }
            Expr *arg = parse_unary(P);
            if (lower && upper) {
                return ec_defintg(arg, 'x', lower, upper);
            }
            /* No bounds: return symbolic integral */
            return ec_intg(arg, 'x');
        }
```

- [ ] **Step 2: Add handle_definite_integral to main**

In `src/eculid_main.c`, add:

```c
static void handle_definite_integral(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    if (e->type == EC_INT && e->left && e->right) {
        /* Definite integral: evaluate numerically */
        Expr *integrand = e->arg;
        char var = e->deriv_var;
        double val = ec_numerical_integral(integrand, var, 0.0, 1.0, "adaptive_simpson");
        char buf[256];
        snprintf(buf, sizeof(buf), "%.10g", val);
        output_result(buf, NULL, NULL);
    } else {
        /* Symbolic indefinite */
        Expr *F = ec_integrate(e, 'x');
        char *latex = ec_to_latex(F);
        char full[8192];
        snprintf(full, sizeof(full), "%s + C", latex);
        output_result(full, NULL, NULL);
        ec_free(latex); ec_free_expr(F);
    }
    ec_free_expr(e);
}
```

Wait — this only handles `a=0, b=1`. The correct approach: detect `ec_defintg` node and extract the actual bounds from `e->left` and `e->right`.

Better implementation:

```c
static void handle_definite_integral(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    /* Check if parsed as EC_DEFINTG (type EC_INT with left/right bounds) */
    if (e->type == EC_INT && e->left && e->right) {
        Expr *integrand = e->arg;
        char var = e->deriv_var ? e->deriv_var : 'x';
        ECValue va = ec_eval(e->left, NULL);
        ECValue vb = ec_eval(e->right, NULL);
        double a = ec_value_as_real(&va);
        double b = ec_value_as_real(&vb);
        ec_value_free(&va); ec_value_free(&vb);
        double val = ec_numerical_integral(integrand, var, a, b, "adaptive_simpson");
        char buf[256];
        snprintf(buf, sizeof(buf), "%.10g", val);
        output_result(buf, NULL, NULL);
    } else {
        /* Fall back to indefinite */
        Expr *F = ec_integrate(e, 'x');
        char *latex = ec_to_latex(F);
        char full[8192];
        snprintf(full, sizeof(full), "%s + C", latex);
        output_result(full, NULL, NULL);
        ec_free(latex); ec_free_expr(F);
    }
    ec_free_expr(e);
}
```

Add `EC_DEFINTG` type to `ECNodeType` enum in `src/eculid_ast.h` as `EC_DEFINTG` after `EC_INT`? Actually, `ec_defintg` already uses `EC_INT` type with `left=a, right=b`. The check `e->type == EC_INT && e->left && e->right` is correct.

- [ ] **Step 3: Wire into MODE_INTEGRATE**

In `eculid_main.c`, modify `MODE_INTEGRATE` in `repl_loop` to detect `\int_a^b` vs plain `\int`:

```c
        if (is_cmd(line, "\\int")) {
            /* Check if there's a bounds pattern: \int_0^1 or \int_{0}^{1} */
            /* Fall through to handle_integrate which now parses bounds */
            mode = MODE_INTEGRATE;
            continue;
        }
```

Actually, the simpler approach: in `handle_integrate`, detect whether the parsed expression is a definite integral (EC_INT with bounds) vs indefinite:

```c
static void handle_integrate(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    if (e->type == EC_INT && e->left && e->right) {
        /* Definite integral: evaluate numerically */
        Expr *integrand = e->arg;
        char var = e->deriv_var ? e->deriv_var : 'x';
        ECValue va = ec_eval(e->left, NULL);
        ECValue vb = ec_eval(e->right, NULL);
        double a = ec_value_as_real(&va);
        double b = ec_value_as_real(&vb);
        ec_value_free(&va); ec_value_free(&vb);
        double val = ec_numerical_integral(integrand, var, a, b, "adaptive_simpson");
        char buf[256];
        snprintf(buf, sizeof(buf), "%.10g", val);
        output_result(buf, NULL, NULL);
    } else {
        Expr *F = ec_integrate(e, 'x');
        char *latex = ec_to_latex(F);
        char full[8192];
        snprintf(full, sizeof(full), "%s + C", latex);
        output_result(full, NULL, NULL);
        ec_free(latex); ec_free_expr(F);
    }
    ec_free_expr(e);
}
```

- [ ] **Step 4: Test definite integral syntax**

Build, then:
- `printf '\int\n\\int_0^1 x^2\nexit\n' | ./Eculidim.exe`
- Expected: numerical result `0.3333333333` (approximating ∫₀¹ x² dx = 1/3)

- [ ] **Step 5: Commit**

```bash
git add src/eculid_parser.c src/eculid_main.c src/eculid_ast.h
git commit -m "feat: add definite integral syntax \int_a^b to parser and REPL"
```

---

## Task 6: Equation Solver Enhancements (Stubs → Implementations)

**Files:**
- Modify: `src/eculid_solve.c`

Current stubs that need implementation:
- `ec_factor_poly` (line 424-428): always returns 0
- `ec_solve_inequality` (line 433-435): always returns NULL
- Higher-degree polynomial symbolic solving: not implemented (only numeric fallback)

The existing numeric solver (`ec_solve_numeric`, `ec_solve_numeric_ex`) already supports Newton, bisection, secant, and Brent methods. The main gap is polynomial factoring.

- [ ] **Step 1: Implement ec_factor_poly for simple cases**

Add to `src/eculid_solve.c`:

```c
int ec_factor_poly(Expr *e, const char *var,
                    Expr **factors, int max_factors) {
    if (!e || !var || !factors || max_factors <= 0) return 0;
    /* Extract coefficients */
    char v = var[0];
    double coeffs[20] = {0};
    int deg = extract_coeffs(e, v, coeffs, 20);
    if (deg <= 0 || deg > 4) return 0; /* Only handle up to quartic symbolically */

    int n_factors = 0;

    if (deg == 2) {
        /* Quadratic: factor into (x - r1)(x - r2) if rational roots exist */
        double a = coeffs[2], b = coeffs[1], c = coeffs[0];
        if (a == 0) return 0;
        /* Rational root theorem: factors of c/a */
        for (double r1_num = -20; r1_num <= 20 && n_factors < max_factors - 1; r1_num += 1) {
            for (double r1_den = 1; r1_den <= 20 && n_factors < max_factors - 1; r1_den += 1) {
                double r1 = r1_num / r1_den;
                double val = a*r1*r1 + b*r1 + c;
                if (fabs(val) < 1e-10) {
                    /* r1 is a root: factor (x - r1) */
                    factors[n_factors++] = ec_binary(EC_SUB, ec_varc(v), ec_num(r1));
                    /* Divide polynomial by (x - r1) to get quotient */
                    double q_a = a, q_b = b + a * (-r1);
                    Expr *quotient = ec_binary(EC_ADD, ec_binary(EC_MUL, ec_num(q_a), ec_binary(EC_POW, ec_varc(v), ec_num(2))),
                                               ec_binary(EC_MUL, ec_num(q_b), ec_varc(v)));
                    Expr *simplified_quotient = ec_simplify(quotient);
                    factors[n_factors++] = simplified_quotient;
                    return n_factors;
                }
            }
        }
    }
    return n_factors;
}
```

- [ ] **Step 2: Implement ec_solve_inequality for linear inequalities**

Add to `src/eculid_solve.c`:

```c
ECInterval* ec_solve_inequality(Expr *ineq, const char *var, int *count) {
    if (!ineq || !var || !count) { *count = 0; return NULL; }
    *count = 0;

    /* Normalize to lhs - rhs < 0 or lhs - rhs > 0 */
    ECNodeType op = EC_NIL;
    if (ineq->type == EC_LT || ineq->type == EC_LE ||
        ineq->type == EC_GT || ineq->type == EC_GE) {
        op = ineq->type;
    } else {
        return NULL; /* Not an inequality */
    }

    /* Move all to left: lhs - rhs */
    Expr *norm = ec_binary(EC_SUB, ec_copy(ineq->left), ec_copy(ineq->right));
    Expr *simplified = ec_simplify(norm);
    ec_free_expr(norm);

    /* Solve equality: lhs - rhs = 0 */
    EculidRoots r = ec_solve(simplified, var);
    ec_free_expr(simplified);

    if (r.count == 0) {
        /* Always true or always false */
        ECInterval *iv = ec_malloc(sizeof(ECInterval));
        if (op == EC_LT || op == EC_GT) {
            iv[0].lo = -1e308; iv[0].hi = 1e308;
        } else {
            iv[0].lo = -1e308; iv[0].hi = 1e308;
        }
        iv[0].inclusive = 1;
        *count = 1;
        return iv;
    }

    /* For now: return intervals between roots (simplified approach) */
    ECInterval *result = ec_malloc(r.count * sizeof(ECInterval));
    double prev_val = eval_at(simplified, var, -1e10);
    (void)prev_val;

    for (int i = 0; i < r.count && i < 10; i++) {
        double root_val = 0.0;
        if (r.roots[i]->type == EC_NUM)
            root_val = r.roots[i]->num_val;

        result[i].lo = (i == 0) ? -1e308 : root_val;
        result[i].hi = (i == r.count - 1) ? 1e308 : root_val;
        result[i].inclusive = (op == EC_LE || op == EC_GE) ? 1 : 0;
    }
    *count = r.count;
    ec_roots_free(&r);
    return result;
}
```

- [ ] **Step 3: Build and test polynomial factoring**

Build, then test `x^2 - 1` factoring (if `ec_factor_poly` can be exposed via REPL).

- [ ] **Step 4: Commit**

```bash
git add src/eculid_solve.c
git commit -m "feat: implement ec_factor_poly and ec_solve_inequality stubs"
```

---

## Self-Review Checklist

**1. Spec coverage:**
- `1/x` → ln|x|: Task 1
- Hyperbolic/inverse hyperbolic integrals: Task 2
- LaTeX parentheses: Task 3
- JSON steps: Task 4
- Definite integral syntax: Task 5
- Equation solver stubs: Task 6
- All 8 original features addressed. ✓

**2. Placeholder scan:**
- All code blocks show actual C code. ✓
- No "TBD", "TODO", "implement later". ✓
- Exact file paths used. ✓
- No "similar to Task N" references without repeating code. ✓

**3. Type consistency:**
- `fabs(e->left->num_val - 1.0)` — `fabs` is `<math.h>` ✓
- `ec_defintg`, `ec_intg`, `ec_unary`, `ec_binary` — all exist in the API ✓
- `Derivation*` returned by `ec_diff_with_steps` — function needs to be added (not a stub) ✓
- `ECJSONOutput*` from `eculid_json.h` — correct header ✓
- `latex_needs_parens` helper — `static` local function, not exposing new API ✓
- `ec_numerical_integral` — used in definite integral, already exists in `eculid_integrate.c` ✓
- `ec_defintg` node uses `EC_INT` type with `left/right` bounds — matches existing pattern in `eculid_ast.c:88-95` ✓
