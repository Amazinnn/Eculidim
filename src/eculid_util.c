#include "eculid.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

/* strdup/strndup 在某些 MinGW 版本需定义 */
#ifndef strdup
char* strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = ec_malloc(n);
    memcpy(p, s, n);
    return p;
}
#endif

char ec_error[256] = {0};

char* ec_strdup(const char *s) { if (!s) return NULL; size_t n = strlen(s)+1; char *p = ec_malloc(n); memcpy(p, s, n); return p; }
char* ec_strndup(const char *s, size_t n) { if (!s) return NULL; char *p = ec_malloc(n+1); strncpy(p, s, n); p[n] = 0; return p; }
char* ec_strcat3(const char *a, const char *b, const char *c) {
    size_t n = (a ? strlen(a) : 0) + (b ? strlen(b) : 0) + (c ? strlen(c) : 0) + 1;
    char *r = ec_malloc(n); r[0] = 0;
    if (a) strcat(r, a); if (b) strcat(r, b); if (c) strcat(r, c);
    return r;
}
void ec_trim(char *s) {
    if (!s) return;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) *end-- = 0;
}
char* ec_replace(const char *s, const char *from, const char *to) { (void)s; (void)from; (void)to; return ec_strdup(s); }
void* ec_malloc(size_t n) { return malloc(n); }
void* ec_calloc(size_t n, size_t sz) { return calloc(n, sz); }
void* ec_realloc(void *p, size_t n) { return realloc(p, n); }
char* ec_sprintf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    char buf[1024]; vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    return ec_strdup(buf);
}
void ec_error_set(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(ec_error, sizeof(ec_error), fmt, ap); va_end(ap);
}
int ec_error_occurred(void) { return ec_error[0] != 0; }
void ec_error_clear(void) { ec_error[0] = 0; }
const char* ec_error_get(void) { return ec_error; }

/*============================================================
 * 全局 API
 *============================================================*/
const char* ec_version(void) { return "1.0.0"; }

const char* ec_supported_funcs(void) {
    return "sin,cos,tan,cot,sec,csc,asin,acos,atan,sinh,cosh,tanh,"
           "exp,ln,log,log10,log2,sqrt,cbrt,abs,sign,floor,ceil,round,trunc,"
           "pi,e,i,inf,factorial,gamma,lgamma,erf,erfc,j0,j1";
}

int ec_is_supported(const char *latex_cmd) {
    if (!latex_cmd || *latex_cmd != '\\') return 0;
    static const char *cmds[] = {
        "sin","cos","tan","cot","sec","csc","asin","acos","atan",
        "sinh","cosh","tanh","exp","ln","log","log10","log2",
        "sqrt","cbrt","abs","sign","floor","ceil","round","trunc",
        "frac","pi","e","i","inf","lim","int","sum","prod"
    };
    for (size_t i = 0; i < sizeof(cmds)/sizeof(cmds[0]); i++)
        if (strcmp(latex_cmd + 1, cmds[i]) == 0) return 1;
    return 0;
}
