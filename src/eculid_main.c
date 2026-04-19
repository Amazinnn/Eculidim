#include "eculid.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include <ctype.h>
#include <strings.h>

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
    /* Substitute defined variables from symtab */
    Expr *subbed = ec_copy(e);
    for (int i = 0; i < s->symtab->count; i++) {
        ECSymEntry *entry = s->symtab->entries[i];
        if (entry->type == EC_SYM_VAR && entry->expr) {
            Expr *r = ec_substitute(subbed, entry->name, entry->expr);
            ec_free_expr(subbed);
            subbed = r;
        }
    }
    Expr *simple = ec_simplify(subbed);
    char *latex = ec_to_latex(simple);
    ec_session_set_ans(s, simple);
    ec_session_add(s, input, latex, "eval");
    output_result(latex, NULL, NULL);
    ec_free_expr(simple); ec_free_expr(subbed); ec_free_expr(e); ec_free(latex);
}

static void handle_diff(EculidSession *s, const char *input) {
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    if (s->steps_enabled && g_json_mode) {
        Derivation *d = ec_diff_with_steps(e, 'x');
        ECJSONOutput *o = ec_json_new();
        if (d && d->count > 0)
            ec_json_set_result(o, d->steps[d->count - 1].step_latex);
        for (int i = 0; d && i < d->count; i++)
            ec_json_add_step(o, d->steps[i].step_latex, d->steps[i].rule);
        char *json = ec_json_render(o);
        printf("%s\n", json);
        ec_free(json); ec_json_free(o);
        if (d) ec_derivation_free(d);
    } else {
        Expr *wrapped = ec_diff_with_latex(e, 'x');
        char *latex = ec_to_latex(wrapped);
        output_result(latex, NULL, NULL);
        ec_free_expr(wrapped->arg); ec_free(wrapped);
        ec_free(latex);
    }
    ec_free_expr(e);
}

static void handle_integrate(EculidSession *s, const char *input) {
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    /* Check for definite integral: EC_INT with left/right bounds */
    if (e->type == EC_INT && e->left && e->right) {
        Expr *integrand = e->arg;
        char var = e->deriv_var ? e->deriv_var : 'x';
        ECValue va = ec_eval(e->left, NULL);
        ECValue vb = ec_eval(e->right, NULL);
        double a = ec_value_as_real(&va);
        double b = ec_value_as_real(&vb);
        ec_value_free(&va); ec_value_free(&vb);
        if (!isnan(a) && !isnan(b)) {
            char buf[256];
            double val = ec_numerical_integral(integrand, var, a, b, "adaptive_simpson");
            if (g_json_mode) {
                ECJSONOutput *o = ec_json_new();
                snprintf(buf, sizeof(buf), "%.10g", val);
                ec_json_set_result(o, buf);
                snprintf(buf, sizeof(buf), "\\int_{%.10g}^{%.10g} ", a, b);
                ec_json_add_step(o, buf, "Definite Integral (numerical)");
                char *json = ec_json_render(o);
                printf("%s\n", json);
                ec_free(json); ec_json_free(o);
            } else {
                snprintf(buf, sizeof(buf), "%.10g", val);
                output_result(buf, NULL, NULL);
            }
        } else {
            output_result(NULL, "EC008", "could not evaluate integral bounds to numbers");
        }
        ec_free_expr(e); return;
    }
    /* Symbolic indefinite integral */
    if (s->steps_enabled && g_json_mode) {
        Derivation *d = ec_integrate_with_steps(e, 'x');
        ECJSONOutput *o = ec_json_new();
        if (d && d->count > 0)
            ec_json_set_result(o, d->steps[d->count - 1].step_latex);
        for (int i = 0; d && i < d->count; i++)
            ec_json_add_step(o, d->steps[i].step_latex, d->steps[i].rule);
        char *json = ec_json_render(o);
        printf("%s\n", json);
        ec_free(json); ec_json_free(o);
        if (d) ec_derivation_free(d);
    } else {
        Expr *F = ec_integrate(e, 'x');
        char *latex = ec_to_latex(F);
        char full[8192];
        snprintf(full, sizeof(full), "%s + C", latex);
        output_result(full, NULL, NULL);
        ec_free(latex); ec_free_expr(F);
    }
    ec_free_expr(e);
}

static void handle_solve(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        set_error_code("EC001");
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(e); return;
    }
    /* Route by relation node type */
    if (e->type == EC_LT || e->type == EC_LE || e->type == EC_GT || e->type == EC_GE) {
        int count = 0;
        ECInterval *iv = ec_solve_inequality(e, "x", &count);
        if (g_json_mode) {
            printf("{\n  \"result\": {\"intervals\": %d}, \"steps\": null, \"error\": null\n}\n", count);
        } else {
            if (count == 0) {
                printf("无解\n");
            } else {
                printf("解区间: ");
                for (int i = 0; i < count; i++) {
                    char *lo_s = ec_to_latex(iv[i].lo);
                    char *hi_s = ec_to_latex(iv[i].hi);
                    int inc = iv[i].inclusive;
                    const char *lop = inc ? "[" : "(";
                    const char *hip = inc ? "]" : ")";
                    if (strcmp(hi_s, "\\infty") == 0)
                        printf("%s%s, \\infty%s", i > 0 ? " ∪ " : "", lo_s, hip);
                    else if (strcmp(lo_s, "-\\infty") == 0 || strcmp(lo_s, "\\infty") == 0)
                        printf("%s, %s%s", lo_s, hi_s, hip);
                    else
                        printf("%s%s%s, %s%s", i > 0 ? " ∪ " : "", lop, lo_s, hi_s, hip);
                    ec_free(lo_s); ec_free(hi_s);
                }
                printf("\n");
            }
        }
        ec_interval_free(iv, count);
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
    if (!input || !*input) {
        output_result(NULL, "EC007", "\\sum 语法: \\sum n=m..k expr  (如 \\sum n=1..10 n^2)");
        return;
    }
    /* Try to parse explicit bounds: "n=1..10 n^2" */
    char var = 'n';
    int lo = 1, hi = 10;
    const char *expr_str = input;
    const char *eq = strchr(input, '=');
    const char *dots = strstr(input, "..");
    if (eq && dots && eq < dots) {
        /* Extract variable: "n" from "n=1..10 n^2" */
        while (*input == ' ') input++;
        if (isalpha((unsigned char)*input)) {
            var = *input;
            /* Find lo: after '=', before ".." */
            const char *p = eq + 1;
            while (*p == ' ') p++;
            lo = atoi(p);
            /* Find hi: after "..", before space or end */
            const char *h = dots + 2;
            while (*h == ' ') h++;
            const char *hend = h;
            while (*hend && *hend != ' ' && *hend != '\r' && *hend != '\n') hend++;
            char tmp[32];
            int len = hend - h < 31 ? hend - h : 31;
            strncpy(tmp, h, len);
            tmp[len] = 0;
            hi = atoi(tmp);
            expr_str = hend;
            while (*expr_str == ' ') expr_str++;
        }
    }
    if (!expr_str || !*expr_str) {
        output_result(NULL, "EC007", "\\sum 缺少表达式 (如 \\sum n=1..10 n^2)");
        return;
    }
    Expr *e = ec_parse(expr_str);
    if (ec_parse_error()) {
        output_result(NULL, "EC001", ec_parse_error_msg());
        return;
    }
    char var_str[2] = {var, 0};
    Expr *result = ec_sum(ec_copy(e), var_str, ec_num(lo), ec_num(hi));
    char *l = ec_to_latex(result);
    output_result(l, NULL, NULL);
    ec_free_expr(e); ec_free_expr(result); ec_free(l);
}

static void handle_limit(EculidSession *s, const char *input) {
    (void)s;
    if (!input || !*input) {
        output_result(NULL, "EC007", "\\limit 语法: \\limit x->a expr  (如 \\limit x->0 sin(x)/x)");
        return;
    }
    /* Try to parse "var->point expr" */
    char var = 'x';
    Expr *point = ec_num(0);
    const char *expr_str = input;
    const char *arrow = strstr(input, "->");
    if (arrow && arrow > input && isalpha((unsigned char)arrow[-1])) {
        var = arrow[-1];
        const char *pt_start = arrow + 2;
        while (*pt_start == ' ') pt_start++;
        /* Point expression ends at first whitespace or end of string */
        const char *pt_end = pt_start;
        while (*pt_end && *pt_end != ' ' && *pt_end != '\t' && *pt_end != '\r' && *pt_end != '\n')
            pt_end++;
        /* Extract point substring */
        size_t pt_len = pt_end - pt_start;
        if (pt_len > 0) {
            char *pt_buf = ec_malloc(pt_len + 1);
            strncpy(pt_buf, pt_start, pt_len);
            pt_buf[pt_len] = 0;
            Expr *parsed_pt = ec_parse(pt_buf);
            if (parsed_pt && !ec_parse_error()) {
                Expr *simp = ec_simplify(parsed_pt);
                ec_free_expr(point);
                point = simp;
                expr_str = pt_end;
                while (*expr_str == ' ') expr_str++;
            } else {
                ec_free_expr(parsed_pt);
            }
            ec_free(pt_buf);
        }
    }
    if (!expr_str || !*expr_str) {
        output_result(NULL, "EC007", "\\limit 缺少表达式");
        ec_free_expr(point); return;
    }
    Expr *e = ec_parse(expr_str);
    if (ec_parse_error()) {
        output_result(NULL, "EC001", ec_parse_error_msg());
        ec_free_expr(point); return;
    }
    Expr *L = ec_limit(e, var, point, EC_LIMIT_BOTH);
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

    if (ec_session_load(ec_session_symtab(s), NULL)) {
        printf("Previous session loaded.\n");
    }

    printf("Eculidim v1.0 — 输入 \\help 查看帮助\n");

    while (1) {
        printf("%s", mode_prompt(mode));
        if (!fgets(line, sizeof(line), stdin)) break;
        /* strip \r\n\0 from end */
        line[strcspn(line, "\r\n")] = 0;
        if (!*line) continue;

        if (is_cmd(line, "\\quit") || is_cmd(line, "\\exit") || strcmp(line, "exit") == 0) break;
        if (is_cmd(line, "\\help")) { print_help(); continue; }
        /* Combined-mode: \cmd expr (e.g. \int 1/x, \diff x^2, \int_0^1 x^2) */
        if (strncmp(line, "\\diff ", 6) == 0) { handle_diff(s, line + 6); continue; }
        if (strncmp(line, "\\int ", 5) == 0) { handle_integrate(s, line + 5); continue; }
        if (strncmp(line, "\\int", 4) == 0 && (line[4] == '_' || line[4] == '{')) {
            /* Definite integral shorthand: \int_0^1 x^2 — pass full expr to parser */
            handle_integrate(s, line); continue;
        }
        if (strncmp(line, "\\solve ", 7) == 0) { handle_solve(s, line + 7); continue; }
        if (strncmp(line, "\\series ", 8) == 0) { handle_series(s, line + 8); continue; }
        if (strncmp(line, "\\sum ", 5) == 0) { handle_sum(s, line + 5); continue; }
        if (strncmp(line, "\\limit ", 7) == 0) { handle_limit(s, line + 7); continue; }
        if (strncmp(line, "\\normal ", 8) == 0 || strncmp(line, "\\eval ", 6) == 0) {
            const char *p = strchr(line, ' ');
            handle_normal(s, p ? p + 1 : line, MODE_NORMAL); continue;
        }
        /* Pure mode-switch commands (no expression) */
        if (is_cmd(line, "\\diff")) { mode = MODE_DIFF; continue; }
        if (is_cmd(line, "\\int")) { mode = MODE_INTEGRATE; continue; }
        if (is_cmd(line, "\\solve")) { mode = MODE_SOLVE; continue; }
        if (is_cmd(line, "\\series")) { mode = MODE_SERIES; continue; }
        if (is_cmd(line, "\\sum")) { mode = MODE_SUM; continue; }
        if (is_cmd(line, "\\limit")) { mode = MODE_LIMIT; continue; }
        if (is_cmd(line, "\\num")) { mode = MODE_NUMERIC; continue; }
        if (is_cmd(line, "\\normal") || is_cmd(line, "\\eval")) { mode = MODE_NORMAL; continue; }

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
            const char *p = line + 5;
            while (*p == ' ') p++;
            const char *eq = strchr(p, '=');
            if (!eq) {
                output_result(NULL, "EC007", "def syntax: \\def name = expr or \\def f(var) = expr");
                continue;
            }
            char name[128];
            int name_len = (int)(eq - p);
            if (name_len >= (int)sizeof(name)) name_len = (int)sizeof(name) - 1;
            strncpy(name, p, name_len);
            name[name_len] = 0;
            while (name_len > 0 && name[name_len-1] == ' ') name[--name_len] = 0;
            const char *expr_str = eq + 1;
            while (*expr_str == ' ') expr_str++;
            char func_name[64], var_name[64];
            if (sscanf(name, "%63[^(](%63[^)])", func_name, var_name) == 2) {
                ec_symtab_define(s->symtab, func_name, var_name, expr_str);
                char buf[256];
                snprintf(buf, sizeof(buf), "defined %s(%s) = %s", func_name, var_name, expr_str);
                output_result(buf, NULL, NULL);
            } else {
                Expr *val = ec_parse(expr_str);
                if (ec_parse_error()) {
                    output_result(NULL, "EC001", ec_parse_error_msg());
                    ec_free_expr(val);
                } else {
                    ec_symtab_set_var(s->symtab, name, val);
                    char buf[256];
                    snprintf(buf, sizeof(buf), "%s = %s", name, expr_str);
                    output_result(buf, NULL, NULL);
                }
            }
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
    if (argc > 1) {
        if (strcmp(argv[1], "--json") == 0) {
            g_json_mode = 1;
            if (argc > 2) {
                EculidSession *s = ec_session_new();
                handle_normal(s, argv[2], MODE_NORMAL);
                ec_session_free(s);
                return 0;
            }
        } else {
            EculidSession *s = ec_session_new();
            handle_normal(s, argv[1], MODE_NORMAL);
            ec_session_free(s);
            return 0;
        }
    }
    repl_loop();
    return 0;
}

int main(int argc, char **argv) {
    return ec_cli(argc, argv);
}