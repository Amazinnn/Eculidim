#ifndef ECULID_SERIES_H
#define ECULID_SERIES_H

#include "eculid_ast.h"

typedef enum { EC_SERIES_TAYLOR, EC_SERIES_MACLAURIN,
               EC_SERIES_LAGRANGE, EC_SERIES_PUISEUX } ECSeriesType;

Expr* ec_series(Expr *e, const char *var, const Expr *point,
                 int order, ECSeriesType type);
Expr* ec_taylor(Expr *e, const char *var, const Expr *a, int n);
Expr* ec_maclaurin(Expr *e, const char *var, int n);
Expr* ec_series_integral(Expr *e, const char *var, int order);
Expr* ec_series_revert(Expr *s, const char *var, int n);
Derivation* ec_series_with_steps(Expr *e, const char *var,
                                   const Expr *a, int n);

#endif /* ECULID_SERIES_H */
