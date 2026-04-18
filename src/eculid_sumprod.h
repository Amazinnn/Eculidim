#ifndef ECULID_SUMPROD_H
#define ECULID_SUMPROD_H

#include "eculid_ast.h"

Expr* ec_sum(Expr *expr, const char *var, Expr *lo, Expr *hi);
Expr* ec_product(Expr *expr, const char *var, Expr *lo, Expr *hi);
double ec_sum_eval(Expr *expr, const char *var, double lo, double hi);
double ec_product_eval(Expr *expr, const char *var, double lo, double hi);
int    ec_sum_closed_form(Expr *s, char var, Expr **result);
int    ec_known_sum(const char *expr_str, Expr **closed_form);

#endif /* ECULID_SUMPROD_H */
