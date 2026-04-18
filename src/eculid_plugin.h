#ifndef ECULID_PLUGIN_H
#define ECULID_PLUGIN_H

#include "eculid_ast.h"

#define EC_MAX_PLUGINS 32

void  ec_plugin_init(void);
int   ec_plugin_register(const EculidPlugin *p);
Expr* ec_plugin_dispatch(const char *op, const char *input, Expr *ast, void **ctx);
void  ec_plugin_list_all(void);
int   ec_plugin_count(void);
void  ec_plugin_free_all(void);

#endif /* ECULID_PLUGIN_H */
