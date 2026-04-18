#ifndef ECULID_UTIL_H
#define ECULID_UTIL_H

#include <stddef.h>

char* ec_strdup(const char *s);
char* ec_strndup(const char *s, size_t n);
char* ec_strcat3(const char *a, const char *b, const char *c);
void  ec_trim(char *s);
char* ec_replace(const char *s, const char *from, const char *to);
void* ec_malloc(size_t n);
void* ec_calloc(size_t n, size_t sz);
void* ec_realloc(void *p, size_t n);
char* ec_sprintf(const char *fmt, ...);

#define EC_SAFE_FREE(p) do { if ((p)) { ec_free(p); (p) = NULL; } } while(0)

extern char ec_error[256];
void  ec_error_set(const char *fmt, ...);
int   ec_error_occurred(void);
void  ec_error_clear(void);
const char* ec_error_get(void);

#endif /* ECULID_UTIL_H */
