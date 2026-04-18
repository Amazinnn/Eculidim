#include "eculid.h"

Expr* ec_simplify(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_simplify_full(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_expand(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_expand_trig(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_factor(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_factor_int(long n) { (void)n; return ec_num(0); }
Expr* ec_collect(Expr *e, char var) { (void)var; return e ? ec_copy(e) : NULL; }
Expr* ec_combine(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_cancel(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_normalize(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_rationalize(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_trig_reduce(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_trig_expand(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_exp_to_trig(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_trig_to_exp(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_power_simplify(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_log_combine(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_log_expand(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_abs_simplify(Expr *e) { return e ? ec_copy(e) : NULL; }
Expr* ec_rewrite(Expr *e, ECNodeType new_type) {
    (void)new_type; return e ? ec_copy(e) : NULL;
}
int ec_is_simplified(const Expr *e) { (void)e; return 1; }
int ec_is_polynomial(const Expr *e, const char *var) {
    (void)e; (void)var; return 0;
}
int ec_is_rational(const Expr *e, const char *var) {
    (void)e; (void)var; return 0;
}
int ec_polynomial_div(Expr *num, Expr *den, const char *var,
                      Expr **quotient, Expr **remainder) {
    (void)num; (void)den; (void)var; *quotient = NULL; *remainder = NULL; return 0;
}
