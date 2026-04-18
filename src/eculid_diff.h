#ifndef ECULID_DIFF_H
#define ECULID_DIFF_H

#include "eculid_ast.h"

Expr* ec_diff(Expr *e, char var);
Expr* ec_diff_n(Expr *e, char var, int n);
Expr* ec_partial_diff(Expr *e, char var);
Expr* ec_total_diff(Expr *e, const char **vars, int n);
Derivation* ec_diff_with_steps(Expr *e, char var);
char* ec_diff_rule_name(ECNodeType t);

#endif /* ECULID_DIFF_H */
