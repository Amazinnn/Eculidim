#ifndef ECULID_VALIDATOR_H
#define ECULID_VALIDATOR_H

#include "eculid_ast.h"

typedef struct { const char *name; const char *dim; } ECDimEntry;

ValidationResult ec_validate_syntax(const char *latex);
ValidationResult ec_validate_ast(const Expr *e);
int  ec_check_units(const Expr *e, ECDimEntry *vars, int n);
int  ec_check_matrix_dims(const Expr *e);

#endif /* ECULID_VALIDATOR_H */
