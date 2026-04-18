#ifndef ECULID_SYMTAB_H
#define ECULID_SYMTAB_H

#include "eculid_ast.h"

typedef enum { EC_SYM_VAR, EC_SYM_FUNC, EC_SYM_CONST } ECSymType;

typedef struct {
    ECSymType type;
    char *name;
    char **arg_names;
    int n_args;
    Expr *expr;
} ECSymEntry;

typedef struct {
    ECSymEntry *entries[1024];
    int count;
} ECSymTab;

void    ec_symtab_init(ECSymTab *tab);
void    ec_symtab_free(ECSymTab *tab);
int     ec_symtab_define(ECSymTab *tab, const char *name, const char *args,
                         const char *expr_latex);
int     ec_symtab_set_var(ECSymTab *tab, const char *name, double value);
Expr*   ec_symtab_lookup(const ECSymTab *tab, const char *name);
double  ec_symtab_lookup_var(const ECSymTab *tab, const char *name);
void    ec_symtab_list(const ECSymTab *tab, void (*print_fn)(const char*));
int     ec_symtab_remove(ECSymTab *tab, const char *name);
void    ec_symtab_clear(ECSymTab *tab);
void    ec_symtab_init_builtins(ECSymTab *tab);

#endif /* ECULID_SYMTAB_H */
