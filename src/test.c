#include "eculid.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846

static int pass = 0, fail = 0;

#define TEST(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while(0)

#define TEST_DBL(name, actual, expected, tol) do { \
    double a_ = (actual), e_ = (expected); \
    if (fabs(a_ - e_) < (tol)) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (got %g, expected %g)\n", name, a_, e_); fail++; } \
} while(0)

int main(void) {
    printf("=== Eculidim 测试套件 ===\n\n");

    /* 词法分析 */
    {
        Lexer L;
        lex_init(&L, "x + sin(3.14)");
        TEST("lexer num", L.cur.type == TK_VAR);
        (void)L;
    }

    /* 解析 */
    {
        Expr *e = ec_parse("x + 1");
        TEST("parse add", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("x^2 - 2*x + 1");
        TEST("parse pow/sub/mul", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("\\frac{1}{2}");
        TEST("parse frac", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("\\sin(x)");
        TEST("parse sin", e != NULL);
        ec_free_expr(e);
    }

    /* AST */
    {
        Expr *n = ec_num(42);
        TEST("ec_num", n && n->type == EC_NUM && n->num_val == 42.0);
        ec_free_expr(n);
    }

    {
        Expr *v = ec_vars("x");
        TEST("ec_vars", v && v->type == EC_VAR && strcmp(v->var_str, "x") == 0);
        ec_free_expr(v);
    }

    {
        Expr *a = ec_binary(EC_ADD, ec_num(1), ec_num(2));
        TEST("ec_binary", a && a->type == EC_ADD);
        ec_free_expr(a);
    }

    {
        Expr *e = ec_num(0);
        TEST("ec_is_zero", ec_is_zero(e));
        ec_free_expr(e);
    }

    {
        Expr *e = ec_num(1);
        TEST("ec_is_one", ec_is_one(e));
        ec_free_expr(e);
    }

    {
        Expr *e = ec_num(-5);
        TEST("ec_is_negative", ec_is_negative(e));
        ec_free_expr(e);
    }

    {
        Expr *a = ec_num(1), *b = ec_num(2);
        Expr *sum = ec_binary(EC_ADD, a, b);
        Expr *c = ec_copy(sum);
        TEST("ec_copy", c && c->type == EC_ADD);
        ec_free_expr(sum); ec_free_expr(c);
    }

    /* 求值 */
    {
        ECEnv env = ec_env_new();
        ec_env_bind(&env, "x", 3.0);
        Expr *e = ec_num(3.14);
        ECValue v = ec_eval(e, &env);
        TEST("eval num", v.type == EC_VAL_REAL && fabs(v.real - 3.14) < 1e-9);
        ec_free_expr(e); ec_env_free(&env);
    }

    {
        ECValue v = ec_eval_sym(ec_const(EC_PI));
        TEST("eval PI", v.type == EC_VAL_REAL && fabs(v.real - M_PI) < 1e-12);
    }

    /* 初等函数 */
    TEST_DBL("sin_d", ec_sin_d(0), 0, 1e-12);
    TEST_DBL("cos_d", ec_cos_d(0), 1, 1e-12);
    TEST_DBL("exp_d", ec_exp_d(0), 1, 1e-12);
    TEST_DBL("ln_d", ec_ln_d(1), 0, 1e-12);
    TEST_DBL("sqrt_d", ec_sqrt_d(4), 2, 1e-12);
    TEST_DBL("pow_d", ec_pow_d(2, 3), 8, 1e-12);

    /* 阶乘 */
    TEST_DBL("factorial", ec_factorial_d(5), 120, 1e-9);

    /* 复数 */
    {
        Complex a = ec_c(1, 2), b = ec_c(3, 4);
        Complex s = ec_c_add(&a, &b);
        TEST("complex add", s.re == 4 && s.im == 6);
        Complex p = ec_c_mul(&a, &b);
        TEST("complex mul", fabs(p.re - (-5)) < 1e-9 && fabs(p.im - 10) < 1e-9);
    }

    /* 求导 */
    {
        Expr *e = ec_parse("x^2");
        Expr *d = ec_diff(e, 'x');
        char *l = ec_to_latex(d);
        TEST("diff x^2", d != NULL);
        printf("  diff(x^2) = %s\n", l);
        ec_free_expr(e); ec_free_expr(d); ec_free(l);
    }

    /* 化简 */
    {
        Expr *e = ec_parse("x + 0");
        Expr *s = ec_simplify(e);
        TEST("simplify x+0", s != NULL);
        ec_free_expr(e); ec_free_expr(s);
    }

    /* 符号表 */
    {
        ECSymTab tab; ec_symtab_init(&tab);
        ec_symtab_set_var(&tab, "a", 3.14);
        double v = ec_symtab_lookup_var(&tab, "a");
        TEST("symtab var", fabs(v - 3.14) < 1e-9);
        ec_symtab_free(&tab);
    }

    /* LaTeX 输出 */
    {
        Expr *e = ec_binary(EC_ADD, ec_vars("x"), ec_num(1));
        char *l = ec_to_latex(e);
        TEST("latex output", l != NULL && strlen(l) > 0);
        printf("  latex: %s\n", l);
        ec_free_expr(e); ec_free(l);
    }

    /* 推导步骤 */
    {
        Derivation *d = ec_derivation_new();
        ec_derivation_add(d, "x^2", "幂函数求导");
        ec_derivation_add(d, "2x", "化简");
        TEST("derivation", d->count == 2);
        char *r = ec_derivation_render(d);
        TEST("derivation render", r != NULL);
        ec_free(r); ec_derivation_free(d);
    }

    printf("\n=== 结果: %d 通过, %d 失败 ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
