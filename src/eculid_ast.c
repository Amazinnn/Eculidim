#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * AST 节点构造函数
 *============================================================*/
Expr* ec_num(double v) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = EC_NUM; e->num_val = v; e->var_name = 0; e->var_str = NULL;
    e->left = e->right = e->arg = e->cond = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_varc(char c) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = EC_VAR; e->var_name = c; e->var_str = NULL;
    e->num_val = 0; e->left = e->right = e->arg = e->cond = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_vars(const char *s) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = EC_VAR; e->var_str = ec_strdup(s);
    e->var_name = s[0]; e->num_val = 0;
    e->left = e->right = e->arg = e->cond = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_unary(ECNodeType t, Expr *arg) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = t; e->arg = arg;
    e->left = e->right = e->cond = NULL;
    e->num_val = 0; e->var_name = 0; e->var_str = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_binary(ECNodeType t, Expr *left, Expr *right) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = t; e->left = left; e->right = right;
    e->arg = e->cond = NULL;
    e->num_val = 0; e->var_name = 0; e->var_str = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_ternary(ECNodeType t, Expr *cond, Expr *left, Expr *right) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = t; e->cond = cond; e->left = left; e->right = right;
    e->arg = NULL;
    e->num_val = 0; e->var_name = 0; e->var_str = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_const(ECNodeType type) {
    Expr *e = ec_malloc(sizeof(Expr));
    e->type = type; e->num_val = 0; e->var_name = 0; e->var_str = NULL;
    e->left = e->right = e->arg = e->cond = NULL;
    e->deriv_order = 0; e->deriv_var = 0;
    return e;
}

Expr* ec_deriv(Expr *e, char var, int order) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_DERIV; node->arg = e;
    node->deriv_var = var; node->deriv_order = order;
    node->left = node->right = node->cond = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    return node;
}

Expr* ec_intg(Expr *e, char var) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_INT; node->arg = e;
    node->deriv_var = var; node->deriv_order = 0;
    node->left = node->right = node->cond = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    return node;
}

Expr* ec_defintg(Expr *e, char var, Expr *a, Expr *b) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_INT; node->arg = e;
    node->left = a; node->right = b;
    node->deriv_var = var; node->deriv_order = 0;
    node->cond = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    return node;
}

Expr* ec_limit_node(Expr *e, char var, Expr *point) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_LIMIT; node->arg = e; node->cond = point;
    node->deriv_var = var;
    node->left = node->right = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    node->deriv_order = 0;
    return node;
}

Expr* ec_sum_node(Expr *expr, char var, Expr *lo, Expr *hi) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_SUM; node->arg = expr;
    node->left = lo; node->right = hi;
    node->deriv_var = var;
    node->cond = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    node->deriv_order = 0;
    return node;
}

Expr* ec_product_node(Expr *expr, char var, Expr *lo, Expr *hi) {
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_PROD; node->arg = expr;
    node->left = lo; node->right = hi;
    node->deriv_var = var;
    node->cond = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    node->deriv_order = 0;
    return node;
}

Expr* ec_series_node(Expr *e, const char *var, Expr *a, int order) {
    (void)var;
    Expr *node = ec_malloc(sizeof(Expr));
    node->type = EC_SERIES; node->arg = e;
    node->cond = a; node->deriv_order = order;
    node->left = node->right = NULL;
    node->num_val = 0; node->var_name = 0; node->var_str = NULL;
    node->deriv_var = 0;
    return node;
}

/*============================================================
 * 复制 / 释放
 *============================================================*/
static Expr* ec_copy_rec(const Expr *e) {
    if (!e) return NULL;
    Expr *n = ec_malloc(sizeof(Expr));
    *n = *e;
    if (e->var_str) n->var_str = ec_strdup(e->var_str);
    n->left = ec_copy_rec(e->left);
    n->right = ec_copy_rec(e->right);
    n->arg = ec_copy_rec(e->arg);
    n->cond = ec_copy_rec(e->cond);
    return n;
}

Expr* ec_copy(const Expr *e) { return ec_copy_rec(e); }

void ec_free(Expr *e) {
    if (!e) return;
    if (e->var_str) ec_free(e->var_str);
    ec_free(e->left); ec_free(e->right);
    ec_free(e->arg); ec_free(e->cond);
    ec_free(e);
}

/*============================================================
 * 查询
 *============================================================*/
int ec_is_zero(const Expr *e) { return e && e->type == EC_NUM && e->num_val == 0.0; }
int ec_is_one(const Expr *e) { return e && e->type == EC_NUM && e->num_val == 1.0; }
int ec_is_number(const Expr *e) { return e && e->type == EC_NUM; }

int ec_is_const(const Expr *e) {
    if (!e) return 1;
    if (e->type == EC_VAR) return 0;
    return ec_is_const(e->left) && ec_is_const(e->right)
        && ec_is_const(e->arg) && ec_is_const(e->cond);
}

int ec_has_var(const Expr *e, char var) {
    if (!e) return 0;
    if (e->type == EC_VAR && e->var_name == var) return 1;
    return ec_has_var(e->left, var) || ec_has_var(e->right, var)
        || ec_has_var(e->arg, var) || ec_has_var(e->cond, var);
}

int ec_has_var_str(const Expr *e, const char *var) {
    if (!e || !var) return 0;
    if (e->type == EC_VAR && e->var_str && strcmp(e->var_str, var) == 0) return 1;
    return ec_has_var_str(e->left, var) || ec_has_var_str(e->right, var)
        || ec_has_var_str(e->arg, var) || ec_has_var_str(e->cond, var);
}

int ec_is_poly_in(const Expr *e, char var) {
    (void)e; (void)var; return 0; /* TODO */
}

int ec_degree(const Expr *e, char var) {
    (void)e; (void)var; return 0; /* TODO */
}

int ec_is_integer(const Expr *e) {
    if (!e || e->type != EC_NUM) return 0;
    return e->num_val == (int)e->num_val;
}

int ec_is_negative(const Expr *e) {
    return e && e->type == EC_NUM && e->num_val < 0;
}

int ec_is_complex(const Expr *e) {
    if (!e) return 0;
    if (e->type == EC_I || e->type == EC_COMP) return 1;
    return ec_is_complex(e->left) || ec_is_complex(e->right)
        || ec_is_complex(e->arg);
}

/*============================================================
 * 代入
 *============================================================*/
Expr* ec_substitute(const Expr *e, const char *var, const Expr *value) {
    (void)e; (void)var; (void)value; return ec_copy(e); /* TODO */
}

Expr* ec_substitute_val(const Expr *e, const char *var, double value) {
    return ec_substitute(e, var, ec_num(value));
}

Expr* ec_substitute_func(const Expr *e, const char *func, const Expr *replacement) {
    (void)e; (void)func; (void)replacement; return ec_copy(e); /* TODO */
}

/*============================================================
 * 调试
 *============================================================*/
static void ec_print_rec(const Expr *e) {
    if (!e) { printf("nil"); return; }
    switch (e->type) {
        case EC_NUM: printf("%.g", e->num_val); break;
        case EC_VAR: printf("%s", e->var_str ? e->var_str : (&(char){e->var_name})); break;
        case EC_ADD: printf("("); ec_print_rec(e->left); printf(" + "); ec_print_rec(e->right); printf(")"); break;
        case EC_SUB: printf("("); ec_print_rec(e->left); printf(" - "); ec_print_rec(e->right); printf(")"); break;
        case EC_MUL: printf("("); ec_print_rec(e->left); printf(" * "); ec_print_rec(e->right); printf(")"); break;
        case EC_DIV: printf("("); ec_print_rec(e->left); printf(" / "); ec_print_rec(e->right); printf(")"); break;
        case EC_POW: printf("("); ec_print_rec(e->left); printf(" ^ "); ec_print_rec(e->right); printf(")"); break;
        case EC_NEG: printf("-"); ec_print_rec(e->arg); break;
        case EC_SIN: printf("sin("); ec_print_rec(e->arg); printf(")"); break;
        case EC_COS: printf("cos("); ec_print_rec(e->arg); printf(")"); break;
        case EC_EXP: printf("exp("); ec_print_rec(e->arg); printf(")"); break;
        case EC_LN: printf("ln("); ec_print_rec(e->arg); printf(")"); break;
        case EC_SQRT: printf("sqrt("); ec_print_rec(e->arg); printf(")"); break;
        case EC_I: printf("i"); break;
        case EC_PI: printf("pi"); break;
        case EC_E: printf("e"); break;
        case EC_INF: printf("inf"); break;
        default: printf("<op:%d>", e->type); break;
    }
}

void ec_print(const Expr *e) { ec_print_rec(e); printf("\n"); }
void ec_fprint(FILE *fp, const Expr *e) { (void)fp; ec_print_rec(e); fprintf(fp, "\n"); }
