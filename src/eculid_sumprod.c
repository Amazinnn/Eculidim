#include "eculid.h"
#include <math.h>

Expr* ec_sum(Expr *expr, const char *var, Expr *lo, Expr *hi) {
    return ec_sum_node(expr ? ec_copy(expr) : NULL, var[0], lo, hi);
}

Expr* ec_product(Expr *expr, const char *var, Expr *lo, Expr *hi) {
    return ec_product_node(expr ? ec_copy(expr) : NULL, var[0], lo, hi);
}

double ec_sum_eval(Expr *expr, const char *var, double lo, double hi) {
    (void)expr; (void)var; (void)lo; (void)hi; return 0.0; /* TODO */
}

double ec_product_eval(Expr *expr, const char *var, double lo, double hi) {
    (void)expr; (void)var; (void)lo; (void)hi; return 1.0; /* TODO */
}

int ec_sum_closed_form(Expr *s, char var, Expr **result) {
    (void)s; (void)var; *result = NULL; return 0; /* TODO */
}

int ec_known_sum(const char *expr_str, Expr **closed_form) {
    (void)expr_str; *closed_form = NULL; return 0; /* TODO */
}
