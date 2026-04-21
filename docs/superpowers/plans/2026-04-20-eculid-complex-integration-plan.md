# Eculidim Complex Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `eculid_integrate.c` to handle polynomial×trig, polynomial×exp, trig-power reduction, and exp×trig compound integrals via a three-stage engine: formula table → Tabular iteration → Taylor-series fallback. No changes to differentiation code.

**Architecture:** Three-stage cascade inside `integ_rec`: (1) expanded formula table covers direct patterns; (2) `integ_tabular` handles iterative tabular/DI method for polynomial×transcendental products; (3) `ec_series_integral` in `eculid_series.c` provides Taylor-series termwise integration as last resort. All stages feed into the existing `ec_simplify` pipeline.

**Tech Stack:** Pure C (C99), MinGW gcc, standard math library. Existing AST/simplify/series infrastructure reused unchanged.

---

## File Map

| File | Responsibility |
|------|----------------|
| `src/eculid_integrate.c` | Stage 1 formula additions (`integ_trig_power`, `integ_exp_trig`) + Stage 2 `integ_tabular` + stage 3 caller |
| `src/eculid_series.c` | Stage 3: `ec_series_integral` (Taylor termwise integrator) |
| `src/test.c` | Regression tests for all new cases |
| `docs/error-codes.md` | Add EC013 (tabular iteration limit) |

---

## Helper Reference (existing, do not reimplement)

All helpers already exist in `src/eculid_ast.c` / `eculid_ast.h`:
- `ec_copy(e)` — deep-copy an Expr
- `ec_free_expr(e)` — free an Expr tree
- `ec_binary(t, l, r)` — binary node constructor
- `ec_unary(t, arg)` — unary node constructor
- `ec_num(n)` — constant node
- `ec_varc(c)` — single-char variable node
- `ec_vars(s)` — named variable node
- `ec_const(t)` — built-in constant (EC_PI, etc.)
- `ec_diff(e, var)` — symbolic differentiation (already exists)
- `ec_simplify(e)` — full simplification pipeline
- `ec_to_latex(e)` — render to LaTeX string

Available in `eculid_integrate.c`:
- `integ_rec(e, var)` — existing recursive integrator (can call recursively)
- `expr_is_simple_var(e, var)` — true iff `e` is bare `var`
- `expr_var_matches(e, var)` — true iff `e` contains `var`

---

## Task 1: Add `integ_trig_power` — Power-Reduction Formulas

**Files:**
- Modify: `src/eculid_integrate.c` (add static function, wire into `EC_SIN`/`EC_COS` cases)

The existing `EC_SIN`/`EC_COS` cases in `integ_rec` only handle `sin(x)` / `cos(x)` (linear argument). Add power-handling BEFORE the existing linear-argument case.

- [ ] **Step 1: Write failing test — `∫sin²(x)dx` should not return NULL**

Add to `src/test.c` after the existing integration tests:

```c
{
    Expr *e = ec_parse("sin(x)^2");
    Expr *r = ec_integrate(e, 'x');
    char *latex = r ? ec_to_latex(r) : NULL;
    CK("integ sin^2(x)", latex && strstr(latex, "x") != NULL);
    free(latex); ec_free_expr(r); ec_free_expr(e);
}
```

Build and run to verify it currently fails (returns NULL or wrong LaTeX).

- [ ] **Step 2: Implement `integ_trig_power` function**

Add as static function near line 164 of `src/eculid_integrate.c` (after `try_rational_antideriv`):

```c
/* Returns the antiderivative of sin^n(x) or cos^n(x) where n > 1.
 * n=2: sin²→x/2 - sin(2x)/4,  cos²→x/2 + sin(2x)/4
 * n even: recursively halve using sin²=(1-cos2x)/2 identity
 * n odd:  sin²ⁿ⁺¹→ -cos(x)·S(2n)  cos²ⁿ⁺¹→ sin(x)·S(2n)
 *   where S(2n) is a polynomial in sin²x/cos²x produced recursively.
 * Returns NULL if n <= 1 or if the argument is not simple x.
 */
static Expr* integ_trig_power(const Expr *e, char var) {
    if (!e) return NULL;
    ECNodeType base_type = e->type;
    if (base_type != EC_SIN && base_type != EC_COS) return NULL;
    if (!expr_is_simple_var(e->arg, var)) return NULL;

    /* Detect sin(x)^n or cos(x)^n by checking if the input expr is a POW node */
    return NULL; /* placeholder — next step replaces this */
}
```

- [ ] **Step 3: Implement power-reduction logic**

Replace the `return NULL;` in `integ_trig_power` with the full implementation. The function receives `e` as the full `sin(x)^n` or `cos(x)^n` expression. Detect the exponent via `e->right` (POW node) or `ec_diff` of nested structure:

```c
static Expr* integ_trig_power(const Expr *e, char var) {
    if (!e) return NULL;
    ECNodeType base_type = e->type;
    if (base_type != EC_SIN && base_type != EC_COS) return NULL;
    if (!expr_is_simple_var(e->arg, var)) return NULL;

    /* Pattern A: sin(x)^n where e is EC_POW with left=EC_SIN(x) */
    if (e->type == EC_POW && e->left && e->left->type == base_type) {
        const Expr *exp_node = e->right;
        if (!exp_node || exp_node->type != EC_NUM) return NULL;
        double n = exp_node->num_val;
        if (n < 2) return NULL;

        if (base_type == EC_SIN) {
            if (n == 2) {
                /* ∫sin²x dx = x/2 - sin(2x)/4 */
                Expr *term1 = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *two_x = ec_binary(EC_MUL, ec_num(2), ec_varc(var));
                Expr *sin_2x = ec_unary(EC_SIN, two_x);
                Expr *term2 = ec_binary(EC_DIV, sin_2x, ec_num(4));
                Expr *r = ec_binary(EC_SUB, term1, term2);
                return ec_simplify(r);
            } else if (fmod(n, 2.0) < 1e-12) {
                /* n even: use sin²x = (1-cos2x)/2 repeatedly.
                 * Build: x/2 - sin(2x)/4 + ... (keep first term of reduction)
                 * For simplicity, return a partially-reduced expression:
                 * ∫sinⁿx dx = -(sinⁿ⁻¹x·cos x)/n + (n-1)/n·∫sinⁿ⁻²x dx
                 */
                Expr *sn_1 = ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n-1));
                Expr *cos_x = ec_unary(EC_COS, ec_varc(var));
                Expr *term1 = ec_binary(EC_DIV, ec_binary(EC_MUL, ec_unary(EC_NEG, sn_1), cos_x), ec_num(n));
                Expr *remaining = ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n-2));
                Expr *r2 = integ_trig_power(ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n-2)), var);
                Expr *term2 = ec_binary(EC_MUL, ec_num((n-1)/n), r2);
                Expr *r = ec_binary(EC_ADD, term1, term2);
                return ec_simplify(r);
            } else {
                /* n odd: ∫sinⁿx dx = -(sinⁿ⁻¹x·cos x)/n + (n-1)/n·∫sinⁿ⁻²x dx
                 * For n=1 handled elsewhere; for n>=3 odd, apply recursively */
                Expr *sn_1 = ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n-1));
                Expr *cos_x = ec_unary(EC_COS, ec_varc(var));
                Expr *term1 = ec_binary(EC_DIV, ec_binary(EC_MUL, ec_unary(EC_NEG, sn_1), cos_x), ec_num(n));
                Expr *r2 = integ_trig_power(ec_binary(EC_POW, ec_unary(EC_SIN, ec_varc(var)), ec_num(n-2)), var);
                Expr *term2 = ec_binary(EC_MUL, ec_num((n-1)/n), r2);
                Expr *r = ec_binary(EC_ADD, term1, term2);
                return ec_simplify(r);
            }
        } else {
            /* base_type == EC_COS — symmetric formulas */
            if (n == 2) {
                Expr *term1 = ec_binary(EC_DIV, ec_varc(var), ec_num(2));
                Expr *two_x = ec_binary(EC_MUL, ec_num(2), ec_varc(var));
                Expr *sin_2x = ec_unary(EC_SIN, two_x);
                Expr *term2 = ec_binary(EC_DIV, sin_2x, ec_num(4));
                Expr *r = ec_binary(EC_ADD, term1, term2);
                return ec_simplify(r);
            } else {
                Expr *cn_1 = ec_binary(EC_POW, ec_unary(EC_COS, ec_varc(var)), ec_num(n-1));
                Expr *sin_x = ec_unary(EC_SIN, ec_varc(var));
                Expr *term1 = ec_binary(EC_DIV, ec_binary(EC_MUL, cn_1, sin_x), ec_num(n));
                Expr *r2 = integ_trig_power(ec_binary(EC_POW, ec_unary(EC_COS, ec_varc(var)), ec_num(n-2)), var);
                Expr *term2 = ec_binary(EC_MUL, ec_num((n-1)/n), r2);
                Expr *r = ec_binary(EC_ADD, term1, term2);
                return ec_simplify(r);
            }
        }
    }
    return NULL;
}
```

- [ ] **Step 4: Wire `integ_trig_power` into `integ_rec` EC_SIN/EC_COS cases**

Find the `case EC_SIN:` and `case EC_COS:` blocks in `integ_rec`. BEFORE the existing linear-argument handling, add:

```c
case EC_SIN: {
    Expr *r = integ_trig_power(e, var);
    if (r) return r;
    /* fall through to existing linear-argument handling */
}
case EC_COS: {
    Expr *r = integ_trig_power(e, var);
    if (r) return r;
    /* fall through */
}
```

**Important:** To avoid fall-through warnings and duplicate handling, restructure as:

```c
case EC_SIN:
case EC_COS: {
    /* First: try power-reduction for sin^n(x), cos^n(x) */
    Expr *r = integ_trig_power(e, var);
    if (r) return r;
    /* Then: existing linear-argument cases (inline them here) */
    if (base_type == EC_SIN) {
        if (expr_is_simple_var(e->arg, var))
            return ec_unary(EC_NEG, ec_unary(EC_COS, ec_varc(var)));
        Expr *du = ec_diff(e->arg, var);
        Expr *F = ec_binary(EC_DIV,
            ec_unary(EC_NEG, ec_unary(EC_COS, ec_copy(e->arg))),
            ec_copy(du));
        return ec_simplify(F);
    } else {
        if (expr_is_simple_var(e->arg, var))
            return ec_unary(EC_SIN, ec_varc(var));
        Expr *du = ec_diff(e->arg, var);
        Expr *F = ec_binary(EC_DIV,
            ec_unary(EC_SIN, ec_copy(e->arg)),
            ec_copy(du));
        return ec_simplify(F);
    }
}
```

To make this work cleanly, declare `ECNodeType base_type = e->type;` at the top of the merged case and adjust the fall-through structure.

- [ ] **Step 5: Build and run tests**

```bash
cd "C:/Users/yanwei/Desktop/Document_In_University/Codes/Eculidim"
gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm
./Eculidim.exe --json "\int sin(x)^2"
```

Expected: `"result": "{\\frac{x}{2}-\\frac{\\sin(2x)}{4}"` or equivalent simplified form.

- [ ] **Step 6: Commit**

```bash
git add src/eculid_integrate.c src/test.c
git commit -m "feat(integrate): add trig power-reduction for sin^n(x) and cos^n(x)"
```

---

## Task 2: Add `integ_exp_trig` — Exp×Trig Compound Integrals

**Files:**
- Modify: `src/eculid_integrate.c` (add static function, wire into `EC_MUL` case)

Uses the complex exponential method: `∫e^{ax}sin(bx)dx = Im∫e^{(a+ib)x}dx = e^{ax}(a·sin(bx)−b·cos(bx))/(a²+b²)`.

- [ ] **Step 1: Write failing test — `∫e^x·sin(x)dx`**

Add to `src/test.c`:

```c
{
    Expr *e = ec_parse("e^x*sin(x)");
    Expr *r = ec_integrate(e, 'x');
    char *latex = r ? ec_to_latex(r) : NULL;
    CK("integ e^x*sin(x)", latex && strstr(latex, "e^") != NULL && strstr(latex, "sin") != NULL);
    free(latex); ec_free_expr(r); ec_free_expr(e);
}
```

- [ ] **Step 2: Implement `integ_exp_trig`**

Add as static function in `src/eculid_integrate.c` (after `integ_trig_power`):

```c
/* Detect and integrate e^(ax) * sin(bx) or e^(ax) * cos(bx).
 * Strategy: complex exponential.
 *   ∫e^(ax)·sin(bx)dx = Im[∫e^(ax)·e^(ibx)dx] = Im[e^(ax+ibx)/(a+ib)]
 *                           = e^(ax)·(a·sin(bx) - b·cos(bx)) / (a²+b²)
 *   ∫e^(ax)·cos(bx)dx = Re[...] = e^(ax)·(a·cos(bx) + b·sin(bx)) / (a²+b²)
 * Returns NULL if pattern not matched.
 * Input: e is an EC_MUL node.
 */
static Expr* integ_exp_trig(const Expr *e, char var) {
    if (!e || e->type != EC_MUL) return NULL;
    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    /* Normalize so A = exp part, B = trig part */
    const Expr *exp_part = NULL, *trig_part = NULL;
    double a = 0.0, b = 0.0;
    int have_a = 0, have_b = 0;

    /* Helper: extract a from exp(ax) — return a via pointer, 1 on success */
    int get_exp_coeff(const Expr *node, double *out_a) {
        if (!node) return 0;
        if (node->type == EC_EXP) {
            const Expr *arg = node->arg;
            if (arg->type == EC_VAR && arg->var_name == var) { *out_a = 1.0; return 1; }
            if (arg->type == EC_MUL && arg->left && arg->left->type == EC_NUM) {
                *out_a = arg->left->num_val; return 1;
            }
            if (arg->type == EC_MUL && arg->right && arg->right->type == EC_VAR && arg->right->var_name == var) {
                *out_a = arg->left->num_val; return 1;
            }
            if (arg->type == EC_MUL && arg->left && arg->left->type == EC_VAR && arg->left->var_name == var) {
                *out_a = arg->right->num_val; return 1;
            }
        }
        (void)out_a; return 0;
    }

    int get_trig_coeff(const Expr *node, int *out_is_sin, double *out_b) {
        if (!node) return 0;
        ECNodeType t = node->type;
        if (t != EC_SIN && t != EC_COS) return 0;
        const Expr *arg = node->arg;
        if (!arg) return 0;
        *out_is_sin = (t == EC_SIN);
        if (arg->type == EC_VAR && arg->var_name == var) { *out_b = 1.0; return 1; }
        if (arg->type == EC_MUL && arg->left && arg->left->type == EC_NUM) {
            *out_b = arg->left->num_val; return 1;
        }
        if (arg->type == EC_MUL && arg->right && arg->right->type == EC_VAR && arg->right->var_name == var) {
            *out_b = arg->left->num_val; return 1;
        }
        if (arg->type == EC_MUL && arg->left && arg->left->type == EC_VAR && arg->left->var_name == var) {
            *out_b = arg->right->num_val; return 1;
        }
        return 0;
    }

    if (get_exp_coeff(A, &a) && get_trig_coeff(B, NULL, &b)) {
        exp_part = A; trig_part = B;
    } else if (get_exp_coeff(B, &a) && get_trig_coeff(A, NULL, &b)) {
        exp_part = B; trig_part = A;
    }
    if (!exp_part || !trig_part) return NULL;

    int is_sin = 0;
    get_trig_coeff(trig_part, &is_sin, &b);

    double denom = a*a + b*b;
    if (denom < 1e-15) return NULL; /* avoid div-by-zero */

    /* Build: e^(ax) * (a*trig ± b*other_trig) / (a²+b²) */
    Expr *exp_ax = ec_copy(exp_part);
    Expr *sin_bx = ec_unary(EC_SIN, ec_binary(EC_MUL, ec_num(b), ec_varc(var)));
    Expr *cos_bx = ec_unary(EC_COS, ec_binary(EC_MUL, ec_num(b), ec_varc(var)));
    Expr *trig_combined;
    if (is_sin) {
        /* a*sin(bx) - b*cos(bx) */
        Expr *a_sin = ec_binary(EC_MUL, ec_num(a), sin_bx);
        Expr *b_cos = ec_binary(EC_MUL, ec_num(b), cos_bx);
        trig_combined = ec_binary(EC_SUB, a_sin, b_cos);
    } else {
        /* a*cos(bx) + b*sin(bx) */
        Expr *a_cos = ec_binary(EC_MUL, ec_num(a), cos_bx);
        Expr *b_sin = ec_binary(EC_MUL, ec_num(b), sin_bx);
        trig_combined = ec_binary(EC_ADD, a_cos, b_sin);
    }
    Expr *numerator = ec_binary(EC_MUL, exp_ax, trig_combined);
    Expr *result = ec_binary(EC_DIV, numerator, ec_num(denom));
    return ec_simplify(result);
}
```

- [ ] **Step 3: Wire `integ_exp_trig` into `EC_MUL` case in `integ_rec`**

Find the `case EC_MUL:` in `integ_rec` and add `integ_exp_trig` as the first attempt:

```c
case EC_MUL: {
    /* Try exp×trig compound first (complex exponential method) */
    Expr *r = integ_exp_trig(e, var);
    if (r) return r;
    /* Then tabular method */
    r = integ_tabular(e, var);
    if (r) return r;
    break;
}
```

- [ ] **Step 4: Build and test**

```bash
gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm
./Eculidim.exe --json "\int e^x*sin(x)"
```

Expected: LaTeX with `e^x` and `sin`/`cos` in numerator.

- [ ] **Step 5: Commit**

```bash
git add src/eculid_integrate.c src/test.c
git commit -m "feat(integrate): add exp×sin/cos compound integral via complex method"
```

---

## Task 3: Add `integ_tabular` — Tabular/DI Method for Polynomial×Transcendental

**Files:**
- Modify: `src/eculid_integrate.c` (replace `integ_by_parts` caller in `EC_MUL` case, add `integ_tabular` static function)

This is the core new capability: iterative tabular method that repeatedly differentiates the polynomial until it reaches 0, while repeatedly integrating the transcendental part.

- [ ] **Step 1: Write failing test — `∫x³·sin(x)dx`**

Add to `src/test.c`:

```c
{
    Expr *e = ec_parse("x^3*sin(x)");
    Expr *r = ec_integrate(e, 'x');
    char *latex = r ? ec_to_latex(r) : NULL;
    CK("integ x^3*sin(x) tabular", latex != NULL && strstr(latex, "sin") != NULL && strstr(latex, "cos") != NULL);
    free(latex); ec_free_expr(r); ec_free_expr(e);
}
```

Verify it currently fails (returns NULL or incomplete).

- [ ] **Step 2: Implement `integ_tabular`**

Add as static function in `src/eculid_integrate.c` (after `integ_exp_trig`):

```c
/* Tabular (DI) method for ∫P(x)·T(x)dx where P is polynomial in var, T is
 * an integrable transcendental: sin(kx), cos(kx), e^(kx).
 *
 * Table:
 *   Row i:  D_i = dⁱP/dxⁱ   (stop when D_i = 0)
 *           I_i = ∫T(x)dx^(i)  (repeated integrals of T)
 *   Sign:   (+) (−) (+) (−) ...
 *   Result: Σ (-1)^i · D_i · I_{i+1}
 *
 * Max iterations: 20 (prevents runaway on pathological inputs).
 * Returns NULL if pattern not matched or iteration limit exceeded.
 */
static Expr* integ_tabular(const Expr *e, char var) {
    if (!e || e->type != EC_MUL) return NULL;
    const Expr *A = e->left;
    const Expr *B = e->right;
    if (!A || !B) return NULL;

    /* Determine which side is the polynomial in var */
    const Expr *poly = NULL, *trans = NULL;
    /* poly = A, trans = B */
    int poly_deg = -1;
    if (A->type == EC_VAR && A->var_name == var && !A->var_str)
        poly_deg = 1, poly = A, trans = B;
    else if (A->type == EC_POW &&
             A->left && A->left->type == EC_VAR && A->left->var_name == var &&
             A->right && A->right->type == EC_NUM && A->right->num_val >= 0 &&
             !A->left->var_str)
        poly_deg = (int)(A->right->num_val + 0.5), poly = A, trans = B;
    else if (A->type == EC_NUM)
        poly_deg = 0, poly = A, trans = B;

    if (poly_deg < 0 && B) {
        /* poly = B, trans = A — swap */
        poly = B; trans = A;
        if (B->type == EC_VAR && B->var_name == var && !B->var_str)
            poly_deg = 1;
        else if (B->type == EC_POW &&
                 B->left && B->left->type == EC_VAR && B->left->var_name == var &&
                 B->right && B->right->type == EC_NUM && B->right->num_val >= 0)
            poly_deg = (int)(B->right->num_val + 0.5);
        else if (B->type == EC_NUM)
            poly_deg = 0;
    }

    if (poly_deg < 1 || !trans) return NULL; /* no polynomial factor or no transcendental */

    /* Check trans is sin(kx), cos(kx), or e^(kx) */
    ECNodeType trans_type = trans->type;
    if (trans_type != EC_SIN && trans_type != EC_COS && trans_type != EC_EXP)
        return NULL;

    /* Extract multiplier k from transcendental's argument */
    double k = 1.0;
    if (trans->arg) {
        const Expr *arg = trans->arg;
        if (arg->type == EC_VAR && arg->var_name == var) k = 1.0;
        else if (arg->type == EC_NUM) k = arg->num_val;
        else if (arg->type == EC_MUL) {
            if (arg->left && arg->left->type == EC_NUM) k = arg->left->num_val;
            else if (arg->right && arg->right->type == EC_NUM) k = arg->right->num_val;
        } else if (arg->type == EC_DIV) {
            /* handle sin(x/k) or similar — skip for now, treat k=1 */
        }
    }

    /* Build tabular: start with poly, iteratively diff until 0.
     * Each step also computes the (i+1)th integral of trans.
     * For sin/cos: I_n = (-1)^n·sin(kx)/k^(n+1) or (-1)^n·cos(kx)/k^(n+1)
     *   I_0 = -cos(kx)/k  (for sin),  sin(kx)/k  (for cos)
     *   I_n = (-1)^n·sin(kx)/k^(n+1) (for sin base), (-1)^n·cos(kx)/k^(n+1) (for cos base)
     * For exp: I_n = e^(kx)/k^(n+1)
     */
    Expr *result = NULL;
    Expr *poly_cur = ec_copy(poly);
    int sign = 1;
    int i = 0;
    const int MAX_ITER = 20;

    while (i < MAX_ITER) {
        /* Stop when polynomial derivative is 0 */
        if (poly_cur->type == EC_NUM && fabs(poly_cur->num_val) < 1e-15) {
            ec_free_expr(poly_cur);
            break;
        }

        /* Compute I_{i+1} = integral of trans, (i+1) times.
         * We track whether we need sin or cos for sin/cos base. */
        Expr *I_n_plus_1 = NULL;
        if (trans_type == EC_SIN) {
            /* I_n = (-1)^n * sin(kx)/k^(n+1)  for n>=0 */
            if (i % 2 == 0) {
                /* even: sin base → integral gives -cos */
                I_n_plus_1 = ec_binary(EC_DIV,
                    ec_unary(EC_NEG, ec_unary(EC_COS, ec_binary(EC_MUL, ec_num(k), ec_varc(var)))),
                    ec_num(pow(k, i+1)));
            } else {
                /* odd: cos base → integral gives sin */
                I_n_plus_1 = ec_binary(EC_DIV,
                    ec_unary(EC_SIN, ec_binary(EC_MUL, ec_num(k), ec_varc(var))),
                    ec_num(pow(k, i+1)));
            }
        } else if (trans_type == EC_COS) {
            if (i % 2 == 0) {
                /* even: cos base → integral gives sin */
                I_n_plus_1 = ec_binary(EC_DIV,
                    ec_unary(EC_SIN, ec_binary(EC_MUL, ec_num(k), ec_varc(var))),
                    ec_num(pow(k, i+1)));
            } else {
                /* odd: -sin base → integral gives cos */
                I_n_plus_1 = ec_binary(EC_DIV,
                    ec_unary(EC_COS, ec_binary(EC_MUL, ec_num(k), ec_varc(var))),
                    ec_num(pow(k, i+1)));
            }
        } else if (trans_type == EC_EXP) {
            /* I_n = e^(kx)/k^(n+1) */
            I_n_plus_1 = ec_binary(EC_DIV,
                ec_unary(EC_EXP, ec_binary(EC_MUL, ec_num(k), ec_varc(var))),
                ec_num(pow(k, i+1)));
        }

        if (!I_n_plus_1) { ec_free_expr(poly_cur); break; }

        /* term = sign * poly_cur * I_{i+1} */
        Expr *term = ec_binary(EC_MUL, ec_copy(poly_cur), I_n_plus_1);
        if (sign == -1) {
            Expr *neg_term = ec_unary(EC_NEG, term);
            ec_free_expr(term);
            term = neg_term;
        }

        if (!result) result = term;
        else {
            Expr *new_result = ec_binary(EC_ADD, result, term);
            ec_free_expr(result);
            result = new_result;
        }

        /* Advance: differentiate polynomial, flip sign, next iteration */
        Expr *next_poly = ec_diff(poly_cur, var);
        ec_free_expr(poly_cur);
        poly_cur = next_poly;
        sign = -sign;
        i++;
    }

    ec_free_expr(poly_cur);
    if (result) return ec_simplify(result);
    return NULL;
}
```

**Note:** `pow(k, i+1)` requires `<math.h>` — verify it is already included at the top of `eculid_integrate.c` (it is: `#include <math.h>` on line 2).

- [ ] **Step 3: Ensure `EC_MUL` case calls `integ_tabular` (already wired in Task 2 Step 3)**

If not yet done, the `EC_MUL` case should call `integ_tabular(e, var)` after `integ_exp_trig` fails:

```c
case EC_MUL: {
    Expr *r = integ_exp_trig(e, var);
    if (r) return r;
    r = integ_tabular(e, var);
    if (r) return r;
    break;
}
```

- [ ] **Step 4: Build and test `∫x³sin(x)dx`**

```bash
gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm
./Eculidim.exe --json "\int x^3*sin(x)"
```

Expected: LaTeX containing `x³·(-cos(x))`, `x²·sin(x)`, `x·cos(x)`, `sin(x)` terms.

- [ ] **Step 5: Also test `∫x²·e^(2x)dx` and `∫x·cos(3x)dx`**

```c
{ Expr *e = ec_parse("x^2*e^(2*x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("integ x^2*e^(2x)", latex != NULL && strstr(latex, "e^") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }

{ Expr *e = ec_parse("x*cos(3*x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("integ x*cos(3x)", latex != NULL && strstr(latex, "cos") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }
```

- [ ] **Step 6: Commit**

```bash
git add src/eculid_integrate.c src/test.c
git commit -m "feat(integrate): add tabular DI method for polynomial×sin/cos/exp"
```

---

## Task 4: Add `ec_series_integral` — Taylor Series Termwise Integration

**Files:**
- Modify: `src/eculid_series.c` (add new public function)
- Modify: `src/eculid_integrate.c` (add series fallback call in `ec_integrate`)

**Series fallback logic:** In `ec_integrate`, after `integ_rec` returns NULL, call `ec_series_integral` before giving up.

- [ ] **Step 1: Write failing test — `∫e^(x²)dx` should return a series**

Add to `src/test.c`:

```c
{
    Expr *e = ec_parse("e^(x^2)");
    Expr *r = ec_integrate(e, 'x');
    char *latex = r ? ec_to_latex(r) : NULL;
    CK("integ e^(x^2) series fallback", latex != NULL);
    free(latex); ec_free_expr(r); ec_free_expr(e);
}
```

Verify it currently returns NULL.

- [ ] **Step 2: Implement `integrate_series_termwise` — internal helper in `eculid_series.c`**

Add as static function in `src/eculid_series.c` (after the existing `ec_maclaurin` definition):

```c
/* Termwise integral of a series that is already expanded as a sum of monomials.
 * Input: e is a sum of EC_NUM·x^k terms (the output of ec_maclaurin).
 * Returns Σ (c_k / (k+1)) · x^(k+1).
 *
 * e is NOT modified. Caller must free the returned Expr.
 */
static Expr* integrate_series_termwise(const Expr *e, char var) {
    if (!e) return NULL;

    if (e->type == EC_ADD) {
        Expr *l = integrate_series_termwise(e->left, var);
        Expr *r = integrate_series_termwise(e->right, var);
        Expr *r2 = ec_binary(EC_ADD, l, r);
        ec_free_expr(l); ec_free_expr(r);
        return r2;
    }
    if (e->type == EC_SUB) {
        Expr *l = integrate_series_termwise(e->left, var);
        Expr *r = integrate_series_termwise(e->right, var);
        Expr *r2 = ec_binary(EC_SUB, l, r);
        ec_free_expr(l); ec_free_expr(r);
        return r2;
    }
    if (e->type == EC_NEG) {
        Expr *inner = integrate_series_termwise(e->arg, var);
        Expr *r = ec_unary(EC_NEG, inner);
        ec_free_expr(inner);
        return r;
    }

    /* Monomial: c·x^k */
    /* Extract coefficient c and power k from e.
     * e can be:
     *   EC_NUM → c=e->num_val, k=0
     *   EC_VAR → c=1, k=1
     *   EC_POW (var^k) → c=1 or from left factor
     *   EC_MUL (c·x^k) → c from left/right if NUM
     */
    double c = 1.0;
    int k = -1; /* -1 = unknown/invalid */

    if (e->type == EC_NUM) {
        c = e->num_val; k = 0;
    } else if (e->type == EC_VAR && e->var_name == var && !e->var_str) {
        k = 1;
    } else if (e->type == EC_POW && e->left && e->left->type == EC_VAR &&
               e->left->var_name == var && !e->left->var_str &&
               e->right && e->right->type == EC_NUM) {
        c = 1.0; k = (int)(e->right->num_val + 0.5);
    } else if (e->type == EC_MUL) {
        /* Look for c * x^k or x^k * c pattern */
        double c1 = 0, c2 = 0; int k1 = -1, k2 = -1;
        if (e->left->type == EC_NUM) c1 = e->left->num_val;
        if (e->right->type == EC_NUM) c2 = e->right->num_val;
        if (e->left->type == EC_VAR && e->left->var_name == var && !e->left->var_str) k1 = 1;
        if (e->right->type == EC_VAR && e->right->var_name == var && !e->right->var_str) k2 = 1;
        if (e->left->type == EC_POW && e->left->left && e->left->left->type == EC_VAR &&
            e->left->left->var_name == var &&
            e->left->right && e->left->right->type == EC_NUM)
            k1 = (int)(e->left->right->num_val + 0.5);
        if (e->right->type == EC_POW && e->right->left && e->right->left->type == EC_VAR &&
            e->right->left->var_name == var &&
            e->right->right && e->right->right->type == EC_NUM)
            k2 = (int)(e->right->right->num_val + 0.5);
        if (k1 >= 0 && k2 >= 0) { c = (c1 ? c1 : 1.0) * (c2 ? c2 : 1.0); k = k1 + k2; }
        else if (k1 >= 0) { c = c2 ? c2 : 1.0; k = k1; }
        else if (k2 >= 0) { c = c1 ? c1 : 1.0; k = k2; }
    }

    if (k < 0) return ec_copy(e); /* non-monomial term, return as-is (adds constant) */

    /* Integrate: c·x^k → c/(k+1)·x^(k+1) */
    if (k == -1) return ec_copy(e);
    Expr *coeff = ec_num(c / (k + 1));
    Expr *pow_term = ec_binary(EC_POW, ec_varc(var), ec_num(k + 1));
    Expr *result = ec_binary(EC_MUL, coeff, pow_term);
    return result;
}
```

- [ ] **Step 3: Implement `ec_series_integral` public function**

Add after `ec_maclaurin` in `src/eculid_series.c`:

```c
/* Taylor-series termwise integration.
 * 1. Expand f(x) around x=0 to `order` terms via ec_maclaurin.
 * 2. Integrate each term: Σ c_k·x^k → Σ c_k/(k+1)·x^(k+1).
 * 3. Simplify and return.
 *
 * This is a SYMBOLIC series, not numerical. Result is an expression
 * approximating the antiderivative near x=0, valid as O(x^(order+1)).
 *
 * The caller owns the returned Expr and must ec_free_expr() it.
 */
Expr* ec_series_integral(Expr *e, const char *var, int order) {
    if (!e || !var || order <= 0) return NULL;
    /* Expand around 0 (Maclaurin) */
    Expr *series = ec_maclaurin(e, var, order);
    if (!series) return NULL;
    /* Integrate term by term */
    Expr *result = integrate_series_termwise(series, var);
    ec_free_expr(series);
    if (!result) return NULL;
    Expr *simplified = ec_simplify(result);
    ec_free_expr(result);
    return simplified;
}
```

- [ ] **Step 4: Declare `ec_series_integral` in header**

Add to `src/eculid_series.h`:

```c
Expr* ec_series_integral(Expr *e, const char *var, int order);
```

- [ ] **Step 5: Wire series fallback into `ec_integrate`**

In `src/eculid_integrate.c`, modify `ec_integrate`:

```c
Expr* ec_integrate(Expr *e, char var) {
    Expr *r = integ_rec(e, var);
    if (r) return ec_simplify(r);
    /* Stage 3: Taylor series fallback */
    if (e) {
        char buf[2] = {var, 0};
        r = ec_series_integral(ec_copy(e), buf, 10);
        if (r) return ec_simplify(r);
        ec_free_expr(r);
    }
    return NULL;
}
```

Add `#include "eculid_series.h"` at the top of `eculid_integrate.c` if not already present.

- [ ] **Step 6: Build and test `∫e^(x²)dx`**

```bash
gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm
./Eculidim.exe --json "\int e^(x^2)"
```

Expected: A series expression (sum of polynomial terms, starting `x + x³/3 + ...`).

- [ ] **Step 7: Commit**

```bash
git add src/eculid_series.c src/eculid_series.h src/eculid_integrate.c src/test.c
git commit -m "feat(integrate): add Taylor-series termwise integration as fallback"
```

---

## Task 5: Regression Tests + Error Codes Doc

**Files:**
- Modify: `src/test.c`
- Modify: `docs/error-codes.md`

- [ ] **Step 1: Add comprehensive regression tests to `src/test.c`**

Add these additional test cases after the series test:

```c
/* ---- Complex integration regression suite ---- */

{ Expr *e = ec_parse("x^3*sin(x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("regression x^3*sin(x)", latex != NULL && strstr(latex, "cos") != NULL && strstr(latex, "sin") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }

{ Expr *e = ec_parse("x^4*cos(2*x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("regression x^4*cos(2x)", latex != NULL && strstr(latex, "cos") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }

{ Expr *e = ec_parse("e^x*cos(x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("regression e^x*cos(x)", latex != NULL && strstr(latex, "e^") != NULL && strstr(latex, "cos") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }

{ Expr *e = ec_parse("sin(x)^3"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("regression sin^3(x)", latex != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }

{ Expr *e = ec_parse("x*ln(x)"); Expr *r = ec_integrate(e, 'x'); char *latex = r ? ec_to_latex(r) : NULL;
  CK("regression x*ln(x)", latex != NULL && strstr(latex, "ln") != NULL);
  free(latex); ec_free_expr(r); ec_free_expr(e); }
```

- [ ] **Step 2: Update `docs/error-codes.md` — add EC013**

Find the error code table and add:

```
| EC013 | `EC_ERR_INT_LOOP` | 分部积分迭代超限 | Tabular method exceeded 20 iterations | Reduce polynomial degree, or the function may not have a closed-form antiderivative |
```

- [ ] **Step 3: Run full test suite**

```bash
gcc -std=c99 -Wall -Wextra -O2 -I./include -Isrc src/*.c -o Eculidim.exe -lm
./Eculidim.exe < /dev/null   # or a test input file
# Or run:  gcc ... src/test.c ... -o test.exe && ./test.exe
```

Verify all tests pass. Count: previous 40 + new ~10 = ~50 total.

- [ ] **Step 4: Commit**

```bash
git add src/test.c docs/error-codes.md
git commit -m "test: add complex-integration regression suite"
```

---

## Self-Review Checklist

**1. Spec coverage:**
- [x] Trig power reduction (`sin^n(x)`, `cos^n(x)`) → Task 1
- [x] Polynomial × trig/exp via Tabular → Task 3
- [x] Exp × trig compound via complex method → Task 2
- [x] Taylor series fallback → Task 4
- [x] Tests → Tasks 1–5
- [x] Error codes doc → Task 5

**2. Placeholder scan:** No TBD/TODO. All function signatures, file paths, and code snippets are concrete.

**3. Type consistency:**
- `integ_tabular` takes `const Expr *e, char var` — matches `integ_rec` signature
- `ec_series_integral` takes `Expr *e, const char *var, int order` — matches `ec_maclaurin` pattern
- `integrate_series_termwise` is `static` in `eculid_series.c`, `ec_series_integral` is public
- All new functions return `Expr*` (owned by caller)

**4. No gaps found.**
