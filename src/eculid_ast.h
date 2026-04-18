#ifndef ECULID_AST_H
#define ECULID_AST_H

#include <stddef.h>
#include <stdio.h>

/*============================================================
 * 节点类型枚举
 *============================================================*/
typedef enum {
    EC_NIL, EC_NUM, EC_VAR, EC_ADD, EC_SUB, EC_MUL, EC_DIV,
    EC_POW, EC_NEG, EC_MOD,
    /* 三角函数 */
    EC_SIN, EC_COS, EC_TAN, EC_COT, EC_SEC, EC_CSC,
    /* 反三角函数 */
    EC_ASIN, EC_ACOS, EC_ATAN, EC_ACOT, EC_ASEC, EC_ACSC,
    /* 双曲三角函数 */
    EC_SINH, EC_COSH, EC_TANH, EC_COTH, EC_SECH, EC_CSCH,
    /* 反双曲三角函数 */
    EC_ASINH, EC_ACOSH, EC_ATANH, EC_ACOTH, EC_ASECH, EC_ACSCH,
    /* 指数与对数 */
    EC_EXP, EC_EXP2, EC_EXPM1,
    EC_LN, EC_LOG, EC_LOG10, EC_LOG2, EC_LN1P,
    /* 幂函数与根 */
    EC_SQRT, EC_CBRT, EC_HYPOT,
    /* 绝对值与符号 */
    EC_ABS, EC_SIGN,
    /* 取整函数 */
    EC_FLOOR, EC_CEIL, EC_ROUND, EC_TRUNC,
    /* 最大最小 */
    EC_MAX, EC_MIN,
    /* 伽马与阶乘 */
    EC_GAMMA, EC_FACTORIAL, EC_LGAMMA,
    /* 误差函数 */
    EC_ERF, EC_ERFC, EC_ERFI,
    /* 贝塞尔函数 */
    EC_J0, EC_J1, EC_JN, EC_Y0, EC_Y1, EC_YN,
    /* 特殊常数函数 */
    EC_DIGAMMA, EC_POLYGAMMA,
    /* 比较与逻辑 */
    EC_LT, EC_LE, EC_GT, EC_GE, EC_EQ, EC_NE,
    EC_LOR, EC_LAND, EC_LNOT,
    /* 微积分 */
    EC_INT, EC_DERIV, EC_PDIFF, EC_LIMIT,
    EC_SUM, EC_PROD, EC_SERIES,
    /* 常量与特殊 */
    EC_I, EC_PI, EC_E, EC_INF, EC_NINF,
    EC_COMP, EC_REM, EC_FDIM
} ECNodeType;

/*============================================================
 * AST 节点
 *============================================================*/
typedef struct Expr {
    ECNodeType type;
    double num_val;       /* 数值常量 */
    char   var_name;      /* 单字符变量 */
    char   *var_str;      /* 多字符变量名 */
    struct Expr *left;
    struct Expr *right;
    struct Expr *arg;     /* 单目函数参数 */
    struct Expr *cond;    /* 条件（积分上下限、极限点等） */
    int    deriv_order;  /* 求导阶数 */
    char   deriv_var;    /* 求导变量 */
} Expr;

/*============================================================
 * 复数
 *============================================================*/
typedef struct { double re; double im; } Complex;

/*============================================================
 * 推导步骤
 *============================================================*/
typedef struct { char *step_latex; char *rule; } DerivationStep;
typedef struct { DerivationStep *steps; int count; int capacity; } Derivation;

/*============================================================
 * 变量绑定与求值环境
 *============================================================*/
typedef struct { const char *name; double value; } ECVarBinding;
typedef struct { ECVarBinding *vars; int n; } ECEnv;

/*============================================================
 * 求值结果
 *============================================================*/
typedef enum {
    EC_VAL_SYMBOLIC, EC_VAL_REAL, EC_VAL_COMPLEX,
    EC_VAL_UNDEFINED, EC_VAL_INF, EC_VAL_ERROR
} ECValType;

typedef struct {
    ECValType type;
    double real, imag;
    Expr *sym;
    char *error;
} ECValue;

/*============================================================
 * 方程求解结果
 *============================================================*/
typedef struct { Expr **roots; int count; char *method; } EculidRoots;
typedef struct { Expr *lo, *hi; int inclusive; } ECInterval;

/*============================================================
 * 插件接口
 *============================================================*/
typedef struct {
    const char *name;
    const char *latex_prefix;
    Expr* (*apply)(const char *input, Expr *ast, void *ctx);
    void (*free_ctx)(void *ctx);
    const char *description;
} EculidPlugin;

/*============================================================
 * 验证结果
 *============================================================*/
typedef struct { int valid; char msg[256]; int err_pos; } ValidationResult;

/*============================================================
 * 会话状态
 *============================================================*/
#define EC_MAX_HISTORY 256
typedef struct {
    char *input;
    char *output;
    char *operation;
} EculidHistoryEntry;

/*============================================================
 * 构造函数
 *============================================================*/
Expr* ec_num(double v);
Expr* ec_varc(char c);
Expr* ec_vars(const char *s);
Expr* ec_unary(ECNodeType t, Expr *arg);
Expr* ec_binary(ECNodeType t, Expr *left, Expr *right);
Expr* ec_ternary(ECNodeType t, Expr *cond, Expr *left, Expr *right);
Expr* ec_const(ECNodeType type);
Expr* ec_deriv(Expr *e, char var, int order);
Expr* ec_intg(Expr *e, char var);
Expr* ec_defintg(Expr *e, char var, Expr *a, Expr *b);
Expr* ec_limit_node(Expr *e, char var, Expr *point);
Expr* ec_sum_node(Expr *expr, char var, Expr *lo, Expr *hi);
Expr* ec_product_node(Expr *expr, char var, Expr *lo, Expr *hi);
Expr* ec_series_node(Expr *e, const char *var, Expr *a, int order);

/*============================================================
 * 复制 / 释放
 *============================================================*/
Expr* ec_copy(const Expr *e);
void  ec_free(void *p);
void  ec_free_expr(Expr *e);

/*============================================================
 * 查询
 *============================================================*/
int   ec_is_zero(const Expr *e);
int   ec_is_one(const Expr *e);
int   ec_is_const(const Expr *e);
int   ec_has_var(const Expr *e, char var);
int   ec_has_var_str(const Expr *e, const char *var);
int   ec_is_poly_in(const Expr *e, char var);
int   ec_degree(const Expr *e, char var);
int   ec_is_number(const Expr *e);
int   ec_is_integer(const Expr *e);
int   ec_is_negative(const Expr *e);
int   ec_is_complex(const Expr *e);

/*============================================================
 * 代入
 *============================================================*/
Expr* ec_substitute(const Expr *e, const char *var, const Expr *value);
Expr* ec_substitute_val(const Expr *e, const char *var, double value);
Expr* ec_substitute_func(const Expr *e, const char *func, const Expr *replacement);

/*============================================================
 * 调试
 *============================================================*/
void  ec_print(const Expr *e);
void  ec_fprint(FILE *fp, const Expr *e);

#endif /* ECULID_AST_H */
