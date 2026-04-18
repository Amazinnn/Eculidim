#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * 符号表
 *============================================================*/
static ECSymEntry* find_entry(const ECSymTab *tab, const char *name) {
    for (int i = 0; i < tab->count; i++)
        if (strcmp(tab->entries[i]->name, name) == 0) return tab->entries[i];
    return NULL;
}

void ec_symtab_init(ECSymTab *tab) { tab->count = 0; }

void ec_symtab_free(ECSymTab *tab) {
    for (int i = 0; i < tab->count; i++) {
        ECSymEntry *e = tab->entries[i];
        ec_free(e->name);
        for (int j = 0; j < e->n_args; j++) ec_free(e->arg_names[j]);
        ec_free(e->arg_names);
        if (e->expr) ec_free(e->expr);
        ec_free(e);
    }
    tab->count = 0;
}

int ec_symtab_define(ECSymTab *tab, const char *name, const char *args,
                     const char *expr_latex) {
    (void)args; (void)expr_latex;
    if (tab->count >= 1024) return 0;
    ECSymEntry *e = ec_malloc(sizeof(ECSymEntry));
    e->type = EC_SYM_FUNC;
    e->name = ec_strdup(name);
    e->arg_names = NULL; e->n_args = 0;
    e->expr = NULL;
    tab->entries[tab->count++] = e;
    return 1;
}

int ec_symtab_set_var(ECSymTab *tab, const char *name, double value) {
    (void)value;
    if (tab->count >= 1024) return 0;
    ECSymEntry *e = ec_malloc(sizeof(ECSymEntry));
    e->type = EC_SYM_VAR;
    e->name = ec_strdup(name);
    e->arg_names = NULL; e->n_args = 0;
    e->expr = NULL;
    tab->entries[tab->count++] = e;
    return 1;
}

Expr* ec_symtab_lookup(const ECSymTab *tab, const char *name) {
    ECSymEntry *e = find_entry(tab, name);
    return e ? e->expr : NULL;
}

double ec_symtab_lookup_var(const ECSymTab *tab, const char *name) {
    ECSymEntry *e = find_entry(tab, name);
    if (e && e->type == EC_SYM_VAR) {
        if (e->expr && e->expr->type == EC_NUM) return e->expr->num_val;
    }
    return 0.0;
}

void ec_symtab_list(const ECSymTab *tab, void (*print_fn)(const char*)) {
    char buf[256];
    for (int i = 0; i < tab->count; i++) {
        ECSymEntry *e = tab->entries[i];
        const char *type_str = e->type == EC_SYM_VAR ? "var" :
                               e->type == EC_SYM_FUNC ? "func" : "const";
        snprintf(buf, sizeof(buf), "%s: %s", e->name, type_str);
        print_fn(buf);
    }
}

int ec_symtab_remove(ECSymTab *tab, const char *name) {
    for (int i = 0; i < tab->count; i++) {
        if (strcmp(tab->entries[i]->name, name) == 0) {
            ECSymEntry *e = tab->entries[i];
            ec_free(e->name);
            for (int j = 0; j < e->n_args; j++) ec_free(e->arg_names[j]);
            ec_free(e->arg_names);
            if (e->expr) ec_free(e->expr);
            ec_free(e);
            for (int j = i; j < tab->count - 1; j++) tab->entries[j] = tab->entries[j+1];
            tab->count--;
            return 1;
        }
    }
    return 0;
}

void ec_symtab_clear(ECSymTab *tab) { ec_symtab_free(tab); }

void ec_symtab_init_builtins(ECSymTab *tab) {
    (void)tab; /* TODO: register \sin, \cos, etc. as builtin functions */
}
