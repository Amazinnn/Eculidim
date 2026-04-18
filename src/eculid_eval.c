#include "eculid.h"
#include <math.h>
#include <string.h>

#define M_PI 3.14159265358979323846
#define M_E 2.71828182845904523536

/*============================================================
 * ECEnv
 *============================================================*/
ECEnv ec_env_new(void) {
    ECEnv env = {NULL, 0};
    return env;
}

void ec_env_free(ECEnv *env) {
    if (env->vars) ec_free(env->vars);
    env->vars = NULL; env->n = 0;
}

void ec_env_bind(ECEnv *env, const char *name, double value) {
    ECVarBinding *new_vars = ec_realloc(env->vars, (env->n + 1) * sizeof(ECVarBinding));
    new_vars[env->n].name = ec_strdup(name);
    new_vars[env->n].value = value;
    env->vars = new_vars;
    env->n++;
}

void ec_env_bind_many(ECEnv *env, const ECVarBinding *bindings, int n) {
    for (int i = 0; i < n; i++)
        ec_env_bind(env, bindings[i].name, bindings[i].value);
}

double ec_env_lookup(const ECEnv *env, const char *name) {
    for (int i = 0; i < env->n; i++)
        if (strcmp(env->vars[i].name, name) == 0) return env->vars[i].value;
    return 0.0;
}

/*============================================================
 * ECValue
 *============================================================*/
int ec_value_is_real(const ECValue *v) {
    return v->type == EC_VAL_REAL || v->type == EC_VAL_UNDEFINED;
}

double ec_value_as_real(const ECValue *v) {
    if (v->type == EC_VAL_REAL) return v->real;
    return 0.0;
}

Complex ec_value_as_complex(const ECValue *v) {
    Complex z = {0, 0};
    if (v->type == EC_VAL_REAL) { z.re = v->real; z.im = 0; }
    else if (v->type == EC_VAL_COMPLEX) { z.re = v->real; z.im = v->imag; }
    return z;
}

void ec_value_free(ECValue *v) {
    if (v->sym) ec_free(v->sym);
    if (v->error) ec_free(v->error);
}

/*============================================================
 * 求值入口
 *============================================================*/
static double lookup_var(const Expr *e, const ECEnv *env) {
    if (!env || !e) return 0;
    (void)e;
    return 0.0;
}

ECValue ec_eval(const Expr *e, const ECEnv *env) {
    ECValue v = {EC_VAL_SYMBOLIC, 0, 0, NULL, NULL};
    if (!e) return v;
    switch (e->type) {
        case EC_NUM: v.type = EC_VAL_REAL; v.real = e->num_val; break;
        case EC_VAR: v.type = EC_VAL_REAL; v.real = lookup_var(e, env); break;
        case EC_I: v.type = EC_VAL_COMPLEX; v.real = 0; v.imag = 1; break;
        case EC_PI: v.type = EC_VAL_REAL; v.real = M_PI; break;
        case EC_E: v.type = EC_VAL_REAL; v.real = M_E; break;
        case EC_INF: v.type = EC_VAL_INF; break;
        default: v.type = EC_VAL_SYMBOLIC; break;
    }
    return v;
}

ECValue ec_eval_sym(const Expr *e) {
    return ec_eval(e, NULL);
}

ECValue ec_eval_num(const Expr *e, const ECEnv *env) {
    ECValue v = ec_eval(e, env);
    if (v.type == EC_VAL_SYMBOLIC) v.type = EC_VAL_ERROR;
    return v;
}

ECValue ec_eval_partial(const Expr *e, const ECEnv *env) { return ec_eval(e, env); }
ECValue ec_eval_const(const Expr *e) { return ec_eval(e, NULL); }
ECValue ec_eval_rewrite(const Expr *e, const ECEnv *env, ECNodeType rt) {
    (void)rt; return ec_eval(e, env);
}

/*============================================================
 * 初等函数（纯数值）
 *============================================================*/
double ec_abs_d(double x) { return fabs(x); }
double ec_sign_d(double x) { return x > 0 ? 1 : (x < 0 ? -1 : 0); }
double ec_sqrt_d(double x) { return sqrt(x); }
double ec_cbrt_d(double x) { return cbrt(x); }
double ec_hypot_d(double x, double y) { return hypot(x, y); }
double ec_exp_d(double x) { return exp(x); }
double ec_exp2_d(double x) { return exp2(x); }
double ec_expm1_d(double x) { return expm1(x); }
double ec_ln_d(double x) { return log(x); }
double ec_log_d(double b, double x) { return log(x) / log(b); }
double ec_log10_d(double x) { return log10(x); }
double ec_log2_d(double x) { return log2(x); }
double ec_ln1p_d(double x) { return log1p(x); }
double ec_pow_d(double a, double x) { return pow(a, x); }
double ec_sin_d(double x) { return sin(x); }
double ec_cos_d(double x) { return cos(x); }
double ec_tan_d(double x) { return tan(x); }
double ec_cot_d(double x) { return 1.0 / tan(x); }
double ec_sec_d(double x) { return 1.0 / cos(x); }
double ec_csc_d(double x) { return 1.0 / sin(x); }
double ec_asin_d(double x) { return asin(x); }
double ec_acos_d(double x) { return acos(x); }
double ec_atan_d(double x) { return atan(x); }
double ec_atan2_d(double y, double x) { return atan2(y, x); }
double ec_sinh_d(double x) { return sinh(x); }
double ec_cosh_d(double x) { return cosh(x); }
double ec_tanh_d(double x) { return tanh(x); }
double ec_coth_d(double x) { return 1.0 / tanh(x); }
double ec_sech_d(double x) { return 1.0 / cosh(x); }
double ec_csch_d(double x) { return 1.0 / sinh(x); }
double ec_asinh_d(double x) { return asinh(x); }
double ec_acosh_d(double x) { return acosh(x); }
double ec_atanh_d(double x) { return atanh(x); }
double ec_floor_d(double x) { return floor(x); }
double ec_ceil_d(double x) { return ceil(x); }
double ec_round_d(double x) { return round(x); }
double ec_trunc_d(double x) { return trunc(x); }

/*============================================================
 * 特殊函数
 *============================================================*/
static double gamma_impl(double x) { (void)x; return 0.0; }
static double lgamma_impl(double x) { (void)x; return 0.0; }

double ec_factorial_d(int n) {
    double r = 1.0;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}
double ec_gamma_d(double x) { return gamma_impl(x); }
double ec_lgamma_d(double x) { return lgamma_impl(x); }
double ec_beta_d(double a, double b) { (void)a; (void)b; return 0.0; }
double ec_erf_d(double x) { return erf(x); }
double ec_erfc_d(double x) { return erfc(x); }
double ec_j0_d(double x) { (void)x; return 0.0; }
double ec_j1_d(double x) { (void)x; return 0.0; }
double ec_jn_d(int n, double x) { (void)n; (void)x; return 0.0; }
double ec_digamma_d(double x) { (void)x; return 0.0; }
double ec_polygamma_d(int n, double x) { (void)n; (void)x; return 0.0; }

/*============================================================
 * 复数运算
 *============================================================*/
Complex ec_c(double re, double im) { Complex z = {re, im}; return z; }
int ec_c_is_real(const Complex *z) { return z->im == 0.0; }
double ec_c_abs(const Complex *z) { return hypot(z->re, z->im); }
double ec_c_arg(const Complex *z) { return atan2(z->im, z->re); }
Complex ec_c_conj(const Complex *z) { Complex r = {z->re, -z->im}; return r; }
Complex ec_c_neg(const Complex *z) { Complex r = {-z->re, -z->im}; return r; }
Complex ec_c_add(const Complex *a, const Complex *b) { Complex r = {a->re+b->re, a->im+b->im}; return r; }
Complex ec_c_sub(const Complex *a, const Complex *b) { Complex r = {a->re-b->re, a->im-b->im}; return r; }
Complex ec_c_mul(const Complex *a, const Complex *b) {
    Complex r = {a->re*b->re - a->im*b->im, a->re*b->im + a->im*b->re}; return r;
}
Complex ec_c_div(const Complex *a, const Complex *b) {
    double d = b->re*b->re + b->im*b->im;
    Complex r = {(a->re*b->re + a->im*b->im)/d, (a->im*b->re - a->re*b->im)/d}; return r;
}
Complex ec_c_pow(const Complex *a, const Complex *b) {
    double r = ec_c_abs(a), theta = ec_c_arg(a);
    double lr = pow(r, b->re) * exp(-b->im * theta);
    double th = b->re * theta + b->im * log(r);
    Complex r2 = {lr * cos(th), lr * sin(th)}; return r2;
}
Complex ec_c_sqrt(const Complex *z) {
    double r = ec_c_abs(z), th = ec_c_arg(z) / 2;
    Complex r2 = {r * cos(th), r * sin(th)}; return r2;
}
Complex ec_c_exp(const Complex *z) {
    double v = exp(z->re);
    Complex r = {v * cos(z->im), v * sin(z->im)}; return r;
}
Complex ec_c_log(const Complex *z) {
    Complex r = {log(ec_c_abs(z)), ec_c_arg(z)}; return r;
}
Complex ec_c_sin(const Complex *z) {
    Complex r = {sin(z->re) * cosh(z->im), cos(z->re) * sinh(z->im)}; return r;
}
Complex ec_c_cos(const Complex *z) {
    Complex r = {cos(z->re) * cosh(z->im), -sin(z->re) * sinh(z->im)}; return r;
}
Complex ec_c_sinh(const Complex *z) {
    Complex r = {sinh(z->re) * cos(z->im), cosh(z->re) * sin(z->im)}; return r;
}
Complex ec_c_cosh(const Complex *z) {
    Complex r = {cosh(z->re) * cos(z->im), sinh(z->re) * sin(z->im)}; return r;
}
Complex ec_c_polar(double r, double theta) { Complex z = {r * cos(theta), r * sin(theta)}; return z; }
void ec_c_to_polar(const Complex *z, double *r, double *theta) {
    *r = ec_c_abs(z); *theta = ec_c_arg(z);
}

/*============================================================
 * 多项式
 *============================================================*/
double ec_poly_eval_d(const double *coeffs, int n, double x) {
    double r = 0.0;
    for (int i = n - 1; i >= 0; i--) r = r * x + coeffs[i];
    return r;
}

int ec_poly_solve_quadratic_d(double a, double b, double c,
                               double *r1, double *r2) {
    if (a == 0.0) { if (b == 0.0) return 0; *r1 = -c / b; return 1; }
    double d = b * b - 4 * a * c;
    if (d < 0) return 0;
    if (d == 0) { *r1 = -b / (2 * a); return 1; }
    double sq = sqrt(d);
    *r1 = (-b + sq) / (2 * a); *r2 = (-b - sq) / (2 * a); return 2;
}

int ec_poly_solve_cubic_d(double a, double b, double c, double d,
                            double *r1, double *r2, double *r3) {
    (void)a; (void)b; (void)c; (void)d; (void)r1; (void)r2; (void)r3;
    return 0; /* TODO */
}

/*============================================================
 * 矩阵运算
 *============================================================*/
double* ec_mat_alloc(int rows, int cols) {
    return (double*)ec_calloc((size_t)rows * cols, sizeof(double));
}

double* ec_mat_clone(const double *A, int rows, int cols) {
    double *B = ec_mat_alloc(rows, cols);
    for (int i = 0; i < rows * cols; i++) B[i] = A[i];
    return B;
}

double ec_mat_get(const double *A, int rows, int cols, int i, int j) {
    (void)rows; (void)cols;
    return A[i * cols + j];
}

double ec_mat_det(const double *A, int n) {
    (void)A; (void)n; return 0.0; /* TODO */
}

double* ec_mat_inverse(const double *A, int n) {
    (void)A; (void)n; return NULL; /* TODO */
}

double* ec_mat_mul(const double *A, const double *B, int m, int n, int p) {
    double *C = ec_mat_alloc(m, p);
    for (int i = 0; i < m; i++)
        for (int j = 0; j < p; j++) {
            double s = 0.0;
            for (int k = 0; k < n; k++) s += A[i * n + k] * B[k * p + j];
            C[i * p + j] = s;
        }
    return C;
}

double* ec_mat_solve(const double *A, const double *b, int n) {
    (void)A; (void)b; (void)n; return NULL; /* TODO */
}

double* ec_mat_lu(double *A, int n, int *perm, int *singular) {
    (void)A; (void)n; (void)perm; *singular = 0; return NULL; /* TODO */
}

double* ec_mat_lu_solve(const double *LU, const int *perm, const double *b, int n) {
    (void)LU; (void)perm; (void)b; (void)n; return NULL; /* TODO */
}

double ec_mat_cond(const double *A, int n) {
    (void)A; (void)n; return 0.0; /* TODO */
}

int ec_mat_rank(const double *A, int rows, int cols) {
    (void)A; (void)rows; (void)cols; return 0; /* TODO */
}

void ec_mat_free(double *A) { ec_free(A); }
