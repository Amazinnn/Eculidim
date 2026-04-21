#include "eculid.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846

static int pass = 0, fail = 0;

#define CK(name, cond) do { \
    if (cond) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s\n", name); fail++; } \
} while(0)

#define CK_DBL(name, actual, expected, tol) do { \
    double a_ = (actual), e_ = (expected); \
    if (fabs(a_ - e_) < (tol)) { printf("PASS: %s\n", name); pass++; } \
    else { printf("FAIL: %s (got %g, expected %g)\n", name, a_, e_); fail++; } \
} while(0)

#ifdef TEST
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Eculidim 测试套件 ===\n\n");

    /* 词法分析 */
    {
        Lexer L;
        lex_init(&L, "x + sin(3.14)");
        CK("lexer num", L.cur.type == TK_VAR);
        (void)L;
    }

    /* 解析 */
    {
        Expr *e = ec_parse("x + 1");
        CK("parse add", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("x^2 - 2*x + 1");
        CK("parse pow/sub/mul", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("\\frac{1}{2}");
        CK("parse frac", e != NULL);
        ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("\\sin(x)");
        CK("parse sin", e != NULL);
        ec_free_expr(e);
    }

    /* AST */
    {
        Expr *n = ec_num(42);
        CK("ec_num", n && n->type == EC_NUM && n->num_val == 42.0);
        ec_free_expr(n);
    }

    {
        Expr *v = ec_vars("x");
        CK("ec_vars", v && v->type == EC_VAR && strcmp(v->var_str, "x") == 0);
        ec_free_expr(v);
    }

    {
        Expr *a = ec_binary(EC_ADD, ec_num(1), ec_num(2));
        CK("ec_binary", a && a->type == EC_ADD);
        ec_free_expr(a);
    }

    {
        Expr *e = ec_num(0);
        CK("ec_is_zero", ec_is_zero(e));
        ec_free_expr(e);
    }

    {
        Expr *e = ec_num(1);
        CK("ec_is_one", ec_is_one(e));
        ec_free_expr(e);
    }

    {
        Expr *e = ec_num(-5);
        CK("ec_is_negative", ec_is_negative(e));
        ec_free_expr(e);
    }

    {
        Expr *a = ec_num(1), *b = ec_num(2);
        Expr *sum = ec_binary(EC_ADD, a, b);
        Expr *c = ec_copy(sum);
        CK("ec_copy", c && c->type == EC_ADD);
        ec_free_expr(sum); ec_free_expr(c);
    }

    /* 求值 */
    {
        ECEnv env = ec_env_new();
        ec_env_bind(&env, "x", 3.0);
        Expr *e = ec_num(3.14);
        ECValue v = ec_eval(e, &env);
        CK("eval num", v.type == EC_VAL_REAL && fabs(v.real - 3.14) < 1e-9);
        ec_free_expr(e); ec_env_free(&env);
    }

    {
        ECValue v = ec_eval_sym(ec_const(EC_PI));
        CK("eval PI", v.type == EC_VAL_REAL && fabs(v.real - M_PI) < 1e-12);
    }

    /* 初等函数 */
    CK_DBL("sin_d", ec_sin_d(0), 0, 1e-12);
    CK_DBL("cos_d", ec_cos_d(0), 1, 1e-12);
    CK_DBL("exp_d", ec_exp_d(0), 1, 1e-12);
    CK_DBL("ln_d", ec_ln_d(1), 0, 1e-12);
    CK_DBL("sqrt_d", ec_sqrt_d(4), 2, 1e-12);
    CK_DBL("pow_d", ec_pow_d(2, 3), 8, 1e-12);

    /* 阶乘 */
    CK_DBL("factorial", ec_factorial_d(5), 120, 1e-9);

    /* 复数 */
    {
        Complex a = ec_c(1, 2), b = ec_c(3, 4);
        Complex s = ec_c_add(&a, &b);
        CK("complex add", s.re == 4 && s.im == 6);
        Complex p = ec_c_mul(&a, &b);
        CK("complex mul", fabs(p.re - (-5)) < 1e-9 && fabs(p.im - 10) < 1e-9);
    }

    /* 求导 */
    {
        Expr *e = ec_parse("x^2");
        Expr *d = ec_diff(e, 'x');
        char *l = ec_to_latex(d);
        CK("diff x^2", d != NULL);
        printf("  diff(x^2) = %s\n", l);
        ec_free_expr(e); ec_free_expr(d); ec_free(l);
    }

    /* 化简 */
    {
        Expr *e = ec_parse("x + 0");
        Expr *s = ec_simplify(e);
        CK("simplify x+0", s != NULL);
        ec_free_expr(e); ec_free_expr(s);
    }

    /* 符号表 */
    {
        ECSymTab tab; ec_symtab_init(&tab);
        ec_symtab_set_var(&tab, "a", ec_num(3.14));
        double v = ec_symtab_lookup_var(&tab, "a");
        CK("symtab var", fabs(v - 3.14) < 1e-9);
        ec_symtab_free(&tab);
    }

    /* LaTeX 输出 */
    {
        Expr *e = ec_binary(EC_ADD, ec_vars("x"), ec_num(1));
        char *l = ec_to_latex(e);
        CK("latex output", l != NULL && strlen(l) > 0);
        printf("  latex: %s\n", l);
        ec_free_expr(e); ec_free(l);
    }

    /* 推导步骤 */
    {
        Derivation *d = ec_derivation_new();
        ec_derivation_add(d, "x^2", "幂函数求导");
        ec_derivation_add(d, "2x", "化简");
        CK("derivation", d->count == 2);
        char *r = ec_derivation_render(d);
        CK("derivation render", r != NULL);
        ec_free(r); ec_derivation_free(d);
    }

    /* ---- 关系运算符解析 ---- */
    {
        Expr *e = ec_parse("x+1<3");
        CK("parse relation <", e != NULL && e->type == EC_LT);
        ec_free_expr(e);
    }
    {
        Expr *e = ec_parse("x>=2");
        CK("parse relation >=", e != NULL && e->type == EC_GE);
        ec_free_expr(e);
    }
    {
        Expr *e = ec_parse("x^2-4=0");
        CK("parse relation =", e != NULL && e->type == EC_EQ);
        ec_free_expr(e);
    }
    {
        Expr *e = ec_parse("x!=5");
        CK("parse relation !=", e != NULL && e->type == EC_NE);
        ec_free_expr(e);
    }

    /* ---- Gamma 函数 ---- */
    CK_DBL("gamma(5)", ec_gamma_d(5.0), 24.0, 1e-9);
    CK_DBL("lgamma(5)", ec_lgamma_d(5.0), log(24.0), 1e-9);

    /* ---- 方程求解 ---- */
    {
        Expr *e = ec_parse("x^2-4=0");
        EculidRoots r = ec_solve(e, "x");
        CK("solve quadratic", r.count == 2);
        ec_roots_free(&r); ec_free_expr(e);
    }

    /* ---- 不等式求解 ---- */
    {
        Expr *e = ec_parse("x^2-4<0");
        int count = 0;
        ECInterval *iv = ec_solve_inequality(e, "x", &count);
        CK("solve inequality x^2<4", count > 0);
        ec_interval_free(iv, count); ec_free_expr(e);
    }

    /* ---- 极限 ---- */
    {
        Expr *e = ec_parse("sin(x)");
        Expr *L = ec_limit(e, 'x', ec_num(0), EC_LIMIT_BOTH);
        CK("limit sin(x) x->0", L != NULL);
        ec_free_expr(e); if (L) ec_free_expr(L);
    }

    /* ---- 求和 ---- */
    {
        Expr *e = ec_parse("n^2");
        Expr *s = ec_sum(ec_copy(e), "n", ec_num(1), ec_num(10));
        CK("sum n^2 from 1 to 10", s != NULL);
        ec_free_expr(e); ec_free_expr(s);
    }

    /* ---- 积分回归（复杂函数） ---- */
    {
        Expr *e = ec_parse("\\sin(x)^2");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ sin^2(x)", latex && strstr(latex, "x") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("\\sin(x)^3");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ sin^3(x)", latex != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        printf("DBG: case e^x*sin start\n");
        Expr *e = ec_parse("{e}^x*\\sin(x)");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ e^x*sin(x)", latex && strstr(latex, "e^") != NULL && strstr(latex, "sin") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        printf("DBG: case e^x*cos start\n");
        Expr *e = ec_parse("{e}^x*\\cos(x)");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ e^x*cos(x)", latex && strstr(latex, "e^") != NULL && strstr(latex, "cos") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        printf("DBG: case x^3*sin start\n");
        Expr *e = ec_parse("x^3*\\sin(x)");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ x^3*sin(x) tabular", latex && strstr(latex, "sin") != NULL && strstr(latex, "cos") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("x^2*e^(2*x)");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ x^2*e^(2x)", latex && strstr(latex, "e^") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("x*\\cos(3*x)");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ x*cos(3x)", latex && strstr(latex, "cos") != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    {
        Expr *e = ec_parse("{e}^{x^2}");
        Expr *r = ec_integrate(e, 'x');
        char *latex = r ? ec_to_latex(r) : NULL;
        CK("integ e^(x^2) series fallback", latex != NULL);
        ec_free(latex); ec_free_expr(r); ec_free_expr(e);
    }

    /* ---- Gamma 函数节点 ---- */
    {
        Expr *e = ec_unary(EC_GAMMA, ec_num(5));
        CK("gamma node type", e != NULL && e->type == EC_GAMMA);
        ec_free_expr(e);
    }

    printf("\n=== 结果: %d 通过, %d 失败 ===\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
#endif
