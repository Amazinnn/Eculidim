#ifndef ECULID_EVAL_H
#define ECULID_EVAL_H

#include "eculid_ast.h"

/* ECEnv */
ECEnv  ec_env_new(void);
void   ec_env_free(ECEnv *env);
void   ec_env_bind(ECEnv *env, const char *name, double value);
void   ec_env_bind_many(ECEnv *env, const ECVarBinding *bindings, int n);
double ec_env_lookup(const ECEnv *env, const char *name);

/* ECValue */
int     ec_value_is_real(const ECValue *v);
double  ec_value_as_real(const ECValue *v);
Complex ec_value_as_complex(const ECValue *v);
void    ec_value_free(ECValue *v);

/* 求值入口 */
ECValue ec_eval(const Expr *e, const ECEnv *env);
ECValue ec_eval_sym(const Expr *e);
ECValue ec_eval_num(const Expr *e, const ECEnv *env);
ECValue ec_eval_partial(const Expr *e, const ECEnv *env);
ECValue ec_eval_const(const Expr *e);
ECValue ec_eval_rewrite(const Expr *e, const ECEnv *env, ECNodeType rewrite_type);

/* 初等函数（纯数值） */
double ec_abs_d(double x);      double ec_sign_d(double x);
double ec_sqrt_d(double x);     double ec_cbrt_d(double x);
double ec_hypot_d(double x, double y);
double ec_exp_d(double x);      double ec_exp2_d(double x);
double ec_expm1_d(double x);
double ec_ln_d(double x);       double ec_log_d(double b, double x);
double ec_log10_d(double x);    double ec_log2_d(double x);
double ec_ln1p_d(double x);
double ec_pow_d(double a, double x);
double ec_sin_d(double x);      double ec_cos_d(double x);
double ec_tan_d(double x);      double ec_cot_d(double x);
double ec_sec_d(double x);      double ec_csc_d(double x);
double ec_asin_d(double x);    double ec_acos_d(double x);
double ec_atan_d(double x);    double ec_atan2_d(double y, double x);
double ec_sinh_d(double x);    double ec_cosh_d(double x);
double ec_tanh_d(double x);    double ec_coth_d(double x);
double ec_sech_d(double x);    double ec_csch_d(double x);
double ec_asinh_d(double x);   double ec_acosh_d(double x);
double ec_atanh_d(double x);
double ec_floor_d(double x);   double ec_ceil_d(double x);
double ec_round_d(double x);   double ec_trunc_d(double x);

/* 特殊函数 */
double ec_factorial_d(int n);
double ec_gamma_d(double x);    double ec_lgamma_d(double x);
double ec_beta_d(double a, double b);
double ec_erf_d(double x);     double ec_erfc_d(double x);
double ec_j0_d(double x);      double ec_j1_d(double x);
double ec_jn_d(int n, double x);
double ec_digamma_d(double x); double ec_polygamma_d(int n, double x);

/* 复数运算 */
Complex ec_c(double re, double im);
int     ec_c_is_real(const Complex *z);
double  ec_c_abs(const Complex *z);
double  ec_c_arg(const Complex *z);
Complex ec_c_conj(const Complex *z);
Complex ec_c_neg(const Complex *z);
Complex ec_c_add(const Complex *a, const Complex *b);
Complex ec_c_sub(const Complex *a, const Complex *b);
Complex ec_c_mul(const Complex *a, const Complex *b);
Complex ec_c_div(const Complex *a, const Complex *b);
Complex ec_c_pow(const Complex *a, const Complex *b);
Complex ec_c_sqrt(const Complex *z);
Complex ec_c_exp(const Complex *z);
Complex ec_c_log(const Complex *z);
Complex ec_c_sin(const Complex *z);
Complex ec_c_cos(const Complex *z);
Complex ec_c_sinh(const Complex *z);
Complex ec_c_cosh(const Complex *z);
Complex ec_c_polar(double r, double theta);
void    ec_c_to_polar(const Complex *z, double *r, double *theta);

/* 多项式 */
double  ec_poly_eval_d(const double *coeffs, int n, double x);
int     ec_poly_solve_quadratic_d(double a, double b, double c,
                                   double *r1, double *r2);
int     ec_poly_solve_cubic_d(double a, double b, double c, double d,
                                double *r1, double *r2, double *r3);

/* 矩阵运算 */
double* ec_mat_alloc(int rows, int cols);
double* ec_mat_clone(const double *A, int rows, int cols);
double  ec_mat_get(const double *A, int rows, int cols, int i, int j);
double  ec_mat_det(const double *A, int n);
double* ec_mat_inverse(const double *A, int n);
double* ec_mat_mul(const double *A, const double *B, int m, int n, int p);
double* ec_mat_solve(const double *A, const double *b, int n);
double* ec_mat_lu(double *A, int n, int *perm, int *singular);
double* ec_mat_lu_solve(const double *LU, const int *perm, const double *b, int n);
double  ec_mat_cond(const double *A, int n);
int     ec_mat_rank(const double *A, int rows, int cols);
void    ec_mat_free(double *A);

#endif /* ECULID_EVAL_H */
