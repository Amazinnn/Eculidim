#include "eculid.h"
#include <math.h>
#include <stdlib.h>

EculidRoots ec_solve(Expr *eq, const char *var) {
    (void)eq; (void)var;
    EculidRoots r = {NULL, 0, NULL};
    return r; /* TODO */
}

Expr* ec_solve_linear(Expr *eq, char var) {
    (void)eq; (void)var; return NULL; /* TODO */
}

Expr* ec_solve_quadratic(double a, double b, double c) {
    double d = b * b - 4 * a * c;
    if (d < 0) return NULL; /* 无实根 */
    if (d == 0) return ec_binary(EC_EQ, ec_varc(var), ec_num(-b / (2 * a)));
    /* 返回两根 */
    Expr *r1 = ec_binary(EC_EQ, ec_varc('x'), ec_num((-b + sqrt(d)) / (2 * a)));
    Expr *r2 = ec_binary(EC_EQ, ec_varc('x'), ec_num((-b - sqrt(d)) / (2 * a)));
    return ec_binary(EC_ADD, r1, r2); /* 简化表示 */
}

double* ec_solve_numeric(Expr *eq, const char *var,
                          double x0, double x1, int *n_iter) {
    (void)eq; (void)var; (void)x0; (void)x1;
    *n_iter = 0; return NULL; /* TODO */
}

double ec_solve_numeric_ex(Expr *eq, const char *var,
                            double x0, double x1, ECSolveMethod method,
                            double tol, int max_iter, int *n_iter) {
    (void)eq; (void)var; (void)x0; (void)x1; (void)method;
    (void)tol; (void)max_iter;
    *n_iter = 0; return 0.0; /* TODO */
}

EculidRoots ec_solve_poly(Expr *e, const char *var) {
    (void)e; (void)var;
    EculidRoots r = {NULL, 0, NULL};
    return r; /* TODO */
}

int ec_factor_poly(Expr *e, const char *var,
                    Expr **factors, int max_factors) {
    (void)e; (void)var; (void)factors; (void)max_factors; return 0; /* TODO */
}

ECInterval* ec_solve_inequality(Expr *ineq, const char *var, int *count) {
    (void)ineq; (void)var; *count = 0; return NULL; /* TODO */
}

void ec_roots_free(EculidRoots *r) {
    for (int i = 0; i < r->count; i++) ec_free(r->roots[i]);
    if (r->roots) ec_free(r->roots);
    if (r->method) ec_free(r->method);
    r->roots = NULL; r->count = 0;
}

void ec_interval_free(ECInterval *iv, int n) {
    (void)iv; (void)n;
}
