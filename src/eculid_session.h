#ifndef ECULID_SESSION_H
#define ECULID_SESSION_H

#include "eculid_ast.h"
#include "eculid_symtab.h"

typedef struct {
    EculidHistoryEntry entries[EC_MAX_HISTORY];
    int count;
    Expr *ans;
    int steps_enabled;
    ECSymTab *symtab;
} EculidSession;

EculidSession* ec_session_new(void);
void           ec_session_free(EculidSession *s);
void           ec_session_set_steps(EculidSession *s, int enabled);
int            ec_session_get_steps(const EculidSession *s);
Expr*          ec_session_get_ans(const EculidSession *s);
void           ec_session_set_ans(EculidSession *s, Expr *ans);
void           ec_session_add(EculidSession *s, const char *input,
                              const char *output, const char *op);
void           ec_session_history(const EculidSession *s,
                                   void (*print_fn)(const char*));
ECSymTab*      ec_session_symtab(EculidSession *s);

#endif /* ECULID_SESSION_H */
