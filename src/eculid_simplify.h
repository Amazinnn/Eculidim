#ifndef ECULID_SIMPLIFY_H
#define ECULID_SIMPLIFY_H

#include "eculid_ast.h"

Expr* ec_simplify(Expr *e);
Expr* ec_simplify_full(Expr *e);
Expr* ec_expand(Expr *e);
Expr* ec_expand_trig(Expr *e);
Expr* ec_factor(Expr *e);
Expr* ec_factor_int(long n);
Expr* ec_collect(Expr *e, char var);
Expr* ec_combine(Expr *e);
Expr* ec_cancel(Expr *e);
Expr* ec_normalize(Expr *e);
Expr* ec_rationalize(Expr *e);
Expr* ec_trig_reduce(Expr *e);
Expr* ec_trig_expand(Expr *e);
Expr* ec_exp_to_trig(Expr *e);
Expr* ec_trig_to_exp(Expr *e);
Expr* ec_power_simplify(Expr *e);
Expr* ec_log_combine(Expr *e);
Expr* ec_log_expand(Expr *e);
Expr* ec_abs_simplify(Expr *e);
Expr* ec_rewrite(Expr *e, ECNodeType new_type);
int   ec_is_simplified(const Expr *e);
int   ec_is_polynomial(const Expr *e, const char *var);
int   ec_is_rational(const Expr *e, const char *var);
int   ec_polynomial_div(Expr *num, Expr *den, const char *var,
                         Expr **quotient, Expr **remainder);

#endif /* ECULID_SIMPLIFY_H */
