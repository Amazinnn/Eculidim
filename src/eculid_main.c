#include "eculid.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/*============================================================
 * JSON / Text dual output mode
 *============================================================*/
static int g_json_mode = 0;  /* 0 = text, 1 = JSON */

/*============================================================
 * Error codes
 *============================================================*/
static const char* g_error_code = "EC000";

static void set_error_code(const char *code) { g_error_code = code; }

static void json_escape(const char *s, char *buf, size_t cap) {
    size_t j = 0;
    for (int i = 0; s[i] && j < cap - 1; i++) {
        if (s[i] == '"' || s[i] == '\\') {
            if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = s[i]; }
        } else if (s[i] == '\n') {
            if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = 'n'; }
        } else if (s[i] == '\r') {
            if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = 'r'; }
        } else {
            buf[j++] = s[i];
        }
    }
    buf[j] = 0;
}

static void print_json_result(const char *result, const char *error_code_str, const char *error_msg) {
    printf("{\n");
    if (error_msg) {
        char esc[512];
        json_escape(error_msg, esc, sizeof(esc));
        printf("  \"result\": null,\n  \"steps\": null,\n  \"error\": {\"code\": \"%s\", \"message\": \"%s\"}\n",
               error_code_str, esc);
    } else if (result) {
        char esc[4096];
        json_escape(result, esc, sizeof(esc));
        printf("  \"result\": \"%s\",\n  \"steps\": null,\n  \"error\": null\n", esc);
    } else {
        printf("  \"result\": null,\n  \"steps\": null,\n  \"error\": null\n");
    }
    printf("}\n");
}

static void print_error_text(const char *error_msg) {
    printf("Error: %s\n", error_msg);
}

/*============================================================
 * REPL 模式
 *============================================================*/
typedef enum {
    MODE_NORMAL, MODE_DIFF, MODE_INTEGRATE, MODE_SOLVE,
    MODE_SERIES, MODE_SUM, MODE_LIMIT, MODE_NUMERIC
} REPLMode;

static const char* mode_prompt(REPLMode m) {
    switch (m) {
        case MODE_DIFF:       return "diff> ";
        case MODE_INTEGRATE:  return "int> ";
        case MODE_SOLVE:      return "solve> ";
        case MODE_SERIES:     return "series> ";
        case MODE_SUM:        return "sum> ";
        case MODE_LIMIT:       return "limit> ";
        case MODE_NUMERIC:    return "num> ";
        default:              return "ec> ";
    }
}

static int is_cmd(const char *s, const char *cmd) {
    return strncmp(s, cmd, strlen(cmd)) == 0;
}

static void print_help(void) {
    printf("Eculidim v1.0 — 面向 AI Agent 的数学工具\n\n");
    printf("直接输入表达式进行计算（LaTeX 语法）\n");
    printf("命令:\n");
    printf("  \\diff      进入求导模式\n");
    printf("  \\int       进入积分模式\n");
    printf("  \\solve     进入求解模式\n");
    printf("  \\series    进入级数模式\n");
    printf("  \\sum       进入求和模式\n");
    printf("  \\limit     进入极限模式\n");
    printf("  \\num       进入数值模式\n");
    printf("  \\def x=... 定义变量/函数\n");
    printf("  \\ans       使用上次结果\n");
    printf("  \\hist      历史记录\n");
    printf("  \\steps on|off  推导步骤\n");
    printf("  \\json      切换到 JSON 输出模式\n");
    printf("  \\text      切换到文本输出模式\n");
    printf("  \\quit      退出\n");
    printf("  \\help      显示帮助\n");
}

/*============================================================
 * 输出函数
 *============================================================*/
static void output_result(const char *latex, const char *error_code_str, const char *error_msg) {
    if (g_json_mode) {
        print_json_result(latex, error_code_str, error_msg);
    } else if (error_msg) {
        print_error_text(error_msg);
    } else {
        printf("%s\n", latex);
    }
}

static void output_roots(int count, char **roots, const char *error_code_str, const char *error_msg) {
    (void)error_code_str;
    if (g_json_mode) {
        printf("{\n");
        printf("  \"result\": {");
        printf("\"roots\": [");
        for (int i = 0; i < count; i++) {
            char esc[4096];
            json_escape(roots[i], esc, sizeof(esc));
            printf("%s\"%s\"", i > 0 ? ", " : "", esc);
        }
        printf("], \"count\": %d}, \"steps\": null, \"error\": null\n", count);
        printf("}\n");
    } else if (error_msg) {
        print_error_text(error_msg);
    } else {
        printf("解: %d 个\n", count);
        for (int i = 0; i < count; i++) printf("  %s\n", roots[i]);
    }
}

static void output_numeric(double real, double imag, int is_complex, const char *error_code_str, const char *error_msg) {
    (void)error_code_str;
    if (g_json_mode) {
        printf("{\n");
        if (is_complex) {
            printf("  \"result\": {\"type\": \"complex\", \"real\": %.10g, \"imag\": %.10g},\n", real, imag);
        } else {
            printf("  \"result\": {\"type\": \"real\", \"value\": %.10g},\n", real);
        }
        printf("  \"steps\": null, \"error\": null\n");
        printf("}\n");
    } else if (error_msg) {
        print_error_text(error_msg);
    } else if (is_complex) {
        printf("%.10g + %.10gi\n", real, imag);
    } else {
        printf("%.10g\n", real);
    }
}

/*============================================================
 * Handler 函数
 *============================================================*/
static void handle_normal(EculidSession *s, const char *input, REPLMode mode) {
    (void)mode;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *simple = ec_simplify(e);
    char *latex = ec_to_latex(simple);
    ec_session_set_ans(s, simple);
    ec_session_add(s, input, latex, "eval");
    output_result(latex, NULL, NULL);
    ec_free_expr(simple); ec_free_expr(e); ec_free(latex);
}

static void handle_diff(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *d = ec_diff(e, 'x');
    char *latex = ec_to_latex(d);
    output_result(latex, NULL, NULL);
    ec_free_expr(e); ec_free_expr(d); ec_free(latex);
}

static void handle_integrate(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *F = ec_integrate(e, 'x');
    char *latex = ec_to_latex(F);
    char full[8192];
    snprintf(full, sizeof(full), "%s + C", latex);
    output_result(full, NULL, NULL);
    ec_free_expr(e); ec_free_expr(F); ec_free(latex);
}

static void handle_solve(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    EculidRoots r = ec_solve(e, "x");
    char *roots[64];
    int n = r.count < 64 ? r.count : 64;
    for (int i = 0; i < n; i++) {
        roots[i] = ec_to_latex(r.roots[i]);
    }
    output_roots(n, roots, NULL, NULL);
    for (int i = 0; i < n; i++) ec_free(roots[i]);
    ec_roots_free(&r); ec_free_expr(e);
}

static void handle_series(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *t = ec_maclaurin(e, "x", 5);
    char *l = ec_to_latex(t);
    output_result(l, NULL, NULL);
    ec_free_expr(e); ec_free_expr(t); ec_free(l);
}

static void handle_sum(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *result = ec_sum(ec_copy(e), "n", ec_num(1), ec_num(10));
    char *l = ec_to_latex(result);
    output_result(l, NULL, NULL);
    ec_free_expr(e); ec_free_expr(result); ec_free(l);
}

static void handle_limit(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    Expr *L = ec_limit(e, 'x', ec_num(0), EC_LIMIT_BOTH);
    char *l = L ? ec_to_latex(L) : ec_strdup("不存在");
    if (g_json_mode) {
        if (L) output_result(l, NULL, NULL);
        else output_result(NULL, "EC003", "limit does not exist");
    } else {
        printf("极限 = %s\n", l);
    }
    ec_free_expr(e); if (L) ec_free_expr(L); ec_free(l);
}

static void handle_numeric(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    ECValue v = ec_eval_sym(e);
    if (v.type == EC_VAL_REAL) {
        if (isnan(v.real)) {
            output_result(NULL, "EC004", "division by zero or invalid operation");
        } else {
            output_numeric(v.real, 0.0, 0, NULL, NULL);
        }
    } else if (v.type == EC_VAL_COMPLEX) {
        output_numeric(v.real, v.imag, 1, NULL, NULL);
    } else {
        output_result(NULL, "EC005", "cannot evaluate to numeric value");
    }
    ec_free_expr(e);
}

/*============================================================
 * REPL 主循环
 *============================================================*/
static void repl_puts(const char *s) { puts(s); }

static void repl_loop(void) {
    char line[1024];
    REPLMode mode = MODE_NORMAL;
    EculidSession *s = ec_session_new();
    ec_plugin_init();

    printf("Eculidim v1.0 — 输入 \\help 查看帮助\n");

    while (1) {
        printf("%s", mode_prompt(mode));
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        if (!*line) continue;

        if (strcmp(line, "\\quit") == 0 || strcmp(line, "\\exit") == 0) break;
        if (strcmp(line, "\\help") == 0) { print_help(); continue; }
        if (strcmp(line, "\\diff") == 0) { mode = MODE_DIFF; continue; }
        if (strcmp(line, "\\int") == 0) { mode = MODE_INTEGRATE; continue; }
        if (strcmp(line, "\\solve") == 0) { mode = MODE_SOLVE; continue; }
        if (strcmp(line, "\\series") == 0) { mode = MODE_SERIES; continue; }
        if (strcmp(line, "\\sum") == 0) { mode = MODE_SUM; continue; }
        if (strcmp(line, "\\limit") == 0) { mode = MODE_LIMIT; continue; }
        if (strcmp(line, "\\num") == 0) { mode = MODE_NUMERIC; continue; }
        if (strcmp(line, "\\normal") == 0 || strcmp(line, "\\eval") == 0) { mode = MODE_NORMAL; continue; }

        /* JSON / Text mode toggle */
        if (is_cmd(line, "\\json")) { g_json_mode = 1; continue; }
        if (is_cmd(line, "\\text")) { g_json_mode = 0; continue; }

        if (is_cmd(line, "\\hist")) { ec_session_history(s, repl_puts); continue; }
        if (is_cmd(line, "\\steps ")) {
            if (strstr(line, "on")) ec_session_set_steps(s, 1);
            else ec_session_set_steps(s, 0);
            continue;
        }
        if (is_cmd(line, "\\def ")) {
            /* TODO: handle define */
            continue;
        }
        if (is_cmd(line, "\\ans")) {
            Expr *a = ec_session_get_ans(s);
            if (a) {
                char *l = ec_to_latex(a);
                output_result(l, NULL, NULL);
                ec_free(l);
            } else {
                output_result(NULL, "EC002", "no ans available");
            }
            continue;
        }

        switch (mode) {
            case MODE_DIFF:       handle_diff(s, line); break;
            case MODE_INTEGRATE:  handle_integrate(s, line); break;
            case MODE_SOLVE:      handle_solve(s, line); break;
            case MODE_SERIES:     handle_series(s, line); break;
            case MODE_SUM:        handle_sum(s, line); break;
            case MODE_LIMIT:       handle_limit(s, line); break;
            case MODE_NUMERIC:    handle_numeric(s, line); break;
            default:              handle_normal(s, line, mode); break;
        }
    }

    ec_session_free(s);
}

/*============================================================
 * 主入口
 *============================================================*/
int ec_cli(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--json") == 0) {
        g_json_mode = 1;
    }
    repl_loop();
    return 0;
}

int main(int argc, char **argv) {
    return ec_cli(argc, argv);
}