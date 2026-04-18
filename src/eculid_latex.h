#ifndef ECULID_LATEX_H
#define ECULID_LATEX_H

#include "eculid_ast.h"

typedef struct { const char *latex; ECNodeType type; } ECLatexCmd;

Expr*  ec_parse(const char *latex);
void   ec_parse_free(Expr *e);
int    ec_parse_error(void);
const char* ec_parse_error_msg(void);
int    ec_parse_error_pos(void);
char*  ec_latex_normalize(const char *latex);

char*  ec_to_latex(const Expr *e);
char*  ec_to_latex_pretty(const Expr *e);
char*  ec_to_latex_inline(const Expr *e);
char*  ec_to_latex_tree(const Expr *e);
char*  ec_to_latex_html(const Expr *e);
void   ec_latex_free(char *s);

Derivation* ec_derivation_new(void);
void        ec_derivation_add(Derivation *d, const char *latex, const char *rule);
char*       ec_derivation_render(const Derivation *d);
char*       ec_derivation_render_latex(const Derivation *d);
void        ec_derivation_free(Derivation *d);

const ECLatexCmd* ec_latex_cmd_table(void);
int ec_latex_cmd_count(void);
ECNodeType ec_latex_cmd_lookup(const char *cmd);

#endif /* ECULID_LATEX_H */
