#ifndef ECULID_INTEGRATE_H
#define ECULID_INTEGRATE_H

#include "eculid_ast.h"

typedef enum { EC_INT_SIMPSON, EC_INT_GAUSS5, EC_INT_ROMBERG } ECIntMethod;

Expr* ec_integrate(Expr *e, char var);
Expr* ec_definite_integral(Expr *e, char var, Expr *a, Expr *b);
int   ec_has_integral(Expr *e, char var);
Derivation* ec_integrate_with_steps(Expr *e, char var);
double ec_numerical_integral(Expr *e, char var, double a, double b,
                              const char *method);
double ec_numerical_integral_ex(Expr *e, char var, double a, double b,
                                  ECIntMethod method, double tol);

#endif /* ECULID_INTEGRATE_H */
