#include "eculid.h"

ValidationResult ec_validate_syntax(const char *latex) {
    ValidationResult r = {1, {0}, 0};
    if (!latex) { r.valid = 0; snprintf(r.msg, sizeof(r.msg), "空输入"); return r; }
    Expr *e = ec_parse(latex);
    if (ec_parse_error()) {
        r.valid = 0;
        snprintf(r.msg, sizeof(r.msg), "%s", ec_parse_error_msg());
        r.err_pos = ec_parse_error_pos();
    }
    ec_free(e);
    return r;
}

ValidationResult ec_validate_ast(const Expr *e) {
    ValidationResult r = {1, {0}, 0};
    if (!e) { r.valid = 0; snprintf(r.msg, sizeof(r.msg), "空AST"); return r; }
    /* 检查除零 */
    if (e->type == EC_DIV && ec_is_zero(e->right)) {
        r.valid = 0; snprintf(r.msg, sizeof(r.msg), "除零"); return r;
    }
    /* 检查负数平方根 */
    if (e->type == EC_SQRT && ec_is_negative(e->arg)) {
        /* 可以是复数，标记但不报错 */
    }
    return r;
}

int ec_check_units(const Expr *e, ECDimEntry *vars, int n) {
    (void)e; (void)vars; (void)n; return 1; /* TODO */
}

int ec_check_matrix_dims(const Expr *e) {
    (void)e; return 1; /* TODO */
}
