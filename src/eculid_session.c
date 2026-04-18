#include "eculid.h"

static ECSymTab g_default_symtab;

EculidSession* ec_session_new(void) {
    EculidSession *s = ec_malloc(sizeof(EculidSession));
    s->count = 0; s->ans = NULL; s->steps_enabled = 0;
    ec_symtab_init(&g_default_symtab);
    ec_symtab_init_builtins(&g_default_symtab);
    s->symtab = &g_default_symtab;
    return s;
}

void ec_session_free(EculidSession *s) {
    if (!s) return;
    for (int i = 0; i < s->count; i++) {
        ec_free(s->entries[i].input);
        ec_free(s->entries[i].output);
        ec_free(s->entries[i].operation);
    }
    if (s->ans) ec_free_expr(s->ans);
    ec_symtab_free(&g_default_symtab);
    ec_free(s);
}

void ec_session_set_steps(EculidSession *s, int enabled) { s->steps_enabled = enabled; }
int ec_session_get_steps(const EculidSession *s) { return s->steps_enabled; }

Expr* ec_session_get_ans(const EculidSession *s) { return s->ans; }

void ec_session_set_ans(EculidSession *s, Expr *ans) {
    if (s->ans) ec_free_expr(s->ans);
    s->ans = ec_copy(ans);
}

void ec_session_add(EculidSession *s, const char *input,
                     const char *output, const char *op) {
    if (s->count >= EC_MAX_HISTORY) {
        /* 丢弃最早的 */
        ec_free(s->entries[0].input);
        ec_free(s->entries[0].output);
        ec_free(s->entries[0].operation);
        for (int i = 0; i < s->count - 1; i++) s->entries[i] = s->entries[i+1];
        s->count--;
    }
    s->entries[s->count].input = ec_strdup(input);
    s->entries[s->count].output = ec_strdup(output);
    s->entries[s->count].operation = ec_strdup(op);
    s->count++;
}

void ec_session_history(const EculidSession *s,
                         void (*print_fn)(const char*)) {
    char buf[512];
    for (int i = 0; i < s->count; i++) {
        snprintf(buf, sizeof(buf), "[%d] %s: %s -> %s",
                 i + 1, s->entries[i].operation,
                 s->entries[i].input, s->entries[i].output);
        print_fn(buf);
    }
}

ECSymTab* ec_session_symtab(EculidSession *s) { return s->symtab; }

/*============================================================
 * 全局结果
 *============================================================*/
static EculidSession *g_session = NULL;

EculidResult ec_evaluate(EculidSession *session, const char *input) {
    EculidResult r = {NULL, NULL, NULL, NULL};
    if (!session) {
        if (!g_session) g_session = ec_session_new();
        session = g_session;
    }
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        r.error = ec_strdup(ec_parse_error_msg()); ec_free_expr(e); return r;
    }
    Expr *simple = ec_simplify(e);
    r.result_ast = ec_copy(simple);
    r.output = ec_to_latex(simple);
    ec_session_set_ans(session, simple);
    ec_session_add(session, input, r.output, "eval");
    if (session->steps_enabled) {
        /* TODO: 生成推导步骤 */
    }
    ec_free_expr(simple); ec_free_expr(e);
    return r;
}

void ec_result_free(EculidResult *r) {
    if (r->output) ec_free(r->output);
    if (r->steps) ec_free(r->steps);
    if (r->error) ec_free(r->error);
    if (r->result_ast) ec_free_expr(r->result_ast);
    r->output = r->steps = r->error = NULL;
    r->result_ast = NULL;
}
