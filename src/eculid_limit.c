#include "eculid.h"

Expr* ec_limit(Expr *e, char var, Expr *point, ECLimitDir dir) {
    (void)e; (void)var; (void)point; (void)dir; return NULL; /* TODO */
}

Expr* ec_limit_at_inf(Expr *e, char var, int sign) {
    (void)e; (void)var; (void)sign; return NULL; /* TODO */
}

int ec_limit_exists(Expr *e, char var, Expr *point) {
    (void)e; (void)var; (void)point; return 0; /* TODO */
}

Derivation* ec_limit_with_steps(Expr *e, char var, Expr *point) {
    (void)e; (void)var; (void)point; return NULL; /* TODO */
}
