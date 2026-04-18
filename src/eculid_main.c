#include "eculid.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    printf("  \\quit      退出\n");
    printf("  \\help      显示帮助\n");
}

static void handle_normal(EculidSession *s, const char *input, REPLMode mode) {
    (void)mode;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) {
        printf("解析错误: %s\n", ec_parse_error_msg());
        ec_free(e); return;
    }
    Expr *simple = ec_simplify(e);
    char *latex = ec_to_latex(simple);
    printf("%s\n", latex);
    ec_session_set_ans(s, simple);
    ec_session_add(s, input, latex, "eval");
    ec_free_expr(simple); ec_free_expr(e); ec_free(latex);
}

static void handle_diff(EculidSession *s, const char *input) {
    /* \diff x -> 对 x 求导，或 diff(expr, var) */
    (void)s; (void)input;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    /* 默认对 x 求导 */
    Expr *d = ec_diff(e, 'x');
    char *latex = ec_to_latex(d);
    printf("%s\n", latex);
    ec_free_expr(e); ec_free_expr(d); ec_free(latex);
}

static void handle_integrate(EculidSession *s, const char *input) {
    (void)s;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    Expr *F = ec_integrate(e, 'x');
    char *latex = ec_to_latex(F);
    printf("%s + C\n", latex);
    ec_free_expr(e); ec_free_expr(F); ec_free(latex);
}

static void handle_solve(EculidSession *s, const char *input) {
    (void)s; (void)input;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    EculidRoots r = ec_solve(e, "x");
    printf("解: %d 个\n", r.count);
    for (int i = 0; i < r.count; i++) {
        char *l = ec_to_latex(r.roots[i]); printf("  %s\n", l); ec_free(l);
    }
    ec_roots_free(&r); ec_free_expr(e);
}

static void handle_series(EculidSession *s, const char *input) {
    (void)s; (void)input;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    Expr *t = ec_maclaurin(e, "x", 5);
    char *l = ec_to_latex(t); printf("%s\n", l);
    ec_free_expr(e); ec_free_expr(t); ec_free(l);
}

static void handle_sum(EculidSession *s, const char *input) {
    (void)s; (void)input;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    Expr *result = ec_sum(ec_copy(e), "n", ec_num(1), ec_num(10));
    char *l = ec_to_latex(result); printf("%s\n", l);
    ec_free_expr(e); ec_free_expr(result); ec_free(l);
}

static void handle_limit(EculidSession *s, const char *input) {
    (void)s; (void)input;
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    Expr *L = ec_limit(e, 'x', ec_num(0), EC_LIMIT_BOTH);
    char *l = L ? ec_to_latex(L) : ec_strdup("不存在");
    printf("极限 = %s\n", l);
    ec_free_expr(e); if (L) ec_free_expr(L); ec_free(l);
}

static void handle_numeric(EculidSession *s, const char *input) {
    (void)s;
    /* 支持 {x=1,y=2} 形式的批量代入 */
    Expr *e = ec_parse(input);
    if (ec_parse_error()) { printf("解析错误\n"); ec_free_expr(e); return; }
    ECValue v = ec_eval_sym(e);
    if (v.type == EC_VAL_REAL) printf("%.10g\n", v.real);
    else if (v.type == EC_VAL_COMPLEX) printf("%.10g + %.10gi\n", v.real, v.imag);
    else printf("无法数值化\n");
    ec_free_expr(e);
}

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
        if (is_cmd(line, "\\hist")) { ec_session_history(s, (void(*)(const char*))puts); continue; }
        if (is_cmd(line, "\\steps ")) {
            if (strstr(line, "on")) ec_session_set_steps(s, 1);
            else ec_session_set_steps(s, 0);
            continue;
        }
        if (is_cmd(line, "\\def ")) {
            /* TODO: 处理 define */
            continue;
        }
        if (is_cmd(line, "\\ans")) {
            Expr *a = ec_session_get_ans(s);
            if (a) { char *l = ec_to_latex(a); printf("%s\n", l); ec_free(l); }
            else printf("无 ans\n");
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
int ec_cli(int argc, char *argv[]) { (void)argc; (void)argv; repl_loop(); return 0; }

int main(int argc, char **argv) {
    return ec_cli(argc, argv);
}
