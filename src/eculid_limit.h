#ifndef ECULID_LIMIT_H
#define ECULID_LIMIT_H

#include "eculid_ast.h"

typedef enum { EC_LIMIT_BOTH, EC_LIMIT_LEFT, EC_LIMIT_RIGHT } ECLimitDir;

Expr* ec_limit(Expr *e, char var, Expr *point, ECLimitDir dir);
Expr* ec_limit_at_inf(Expr *e, char var, int sign);
int   ec_limit_exists(Expr *e, char var, Expr *point);
Derivation* ec_limit_with_steps(Expr *e, char var, Expr *point);

#endif /* ECULID_LIMIT_H */
