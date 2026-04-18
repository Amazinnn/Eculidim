#ifndef ECULID_SOLVE_H
#define ECULID_SOLVE_H

#include "eculid_ast.h"

typedef enum { EC_NEWTON, EC_BISECTION, EC_SECANT, EC_BRENTQ } ECSolveMethod;

EculidRoots ec_solve(Expr *eq, const char *var);
Expr*       ec_solve_linear(Expr *eq, char var);
Expr*       ec_solve_quadratic(double a, double b, double c);
double*     ec_solve_numeric(Expr *eq, const char *var,
                               double x0, double x1, int *n_iter);
double ec_solve_numeric_ex(Expr *eq, const char *var,
                             double x0, double x1, ECSolveMethod method,
                             double tol, int max_iter, int *n_iter);
EculidRoots ec_solve_poly(Expr *e, const char *var);
int         ec_factor_poly(Expr *e, const char *var,
                             Expr **factors, int max_factors);
ECInterval* ec_solve_inequality(Expr *ineq, const char *var, int *count);
void        ec_roots_free(EculidRoots *r);
void        ec_interval_free(ECInterval *iv, int n);

#endif /* ECULID_SOLVE_H */
