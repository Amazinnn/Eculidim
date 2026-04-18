#include "eculid.h"

Expr* ec_series(Expr *e, const char *var, const Expr *point,
                 int order, ECSeriesType type) {
    (void)e; (void)var; (void)point; (void)order; (void)type; return NULL; /* TODO */
}

Expr* ec_taylor(Expr *e, const char *var, const Expr *a, int n) {
    (void)e; (void)var; (void)a; (void)n; return NULL; /* TODO */
}

Expr* ec_maclaurin(Expr *e, const char *var, int n) {
    (void)e; (void)var; (void)n; return NULL; /* TODO */
}

Expr* ec_series_revert(Expr *s, const char *var, int n) {
    (void)s; (void)var; (void)n; return NULL; /* TODO */
}

Derivation* ec_series_with_steps(Expr *e, const char *var,
                                  const Expr *a, int n) {
    (void)e; (void)var; (void)a; (void)n; return NULL; /* TODO */
}
