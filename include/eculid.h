#ifndef ECULID_H
#define ECULID_H

/*============================================================
 * 公共类型声明
 * 实际定义见各模块头文件
 *============================================================*/
#include "eculid_ast.h"
#include "eculid_lexer.h"
#include "eculid_parser.h"
#include "eculid_simplify.h"
#include "eculid_diff.h"
#include "eculid_integrate.h"
#include "eculid_solve.h"
#include "eculid_limit.h"
#include "eculid_series.h"
#include "eculid_sumprod.h"
#include "eculid_eval.h"
#include "eculid_latex.h"
#include "eculid_validator.h"
#include "eculid_plugin.h"
#include "eculid_session.h"
#include "eculid_symtab.h"
#include "eculid_util.h"

/*============================================================
 * 主入口 API
 *============================================================*/
const char* ec_version(void);
const char* ec_supported_funcs(void);
int         ec_is_supported(const char *latex_cmd);
int         ec_cli(int argc, char *argv[]);

typedef struct {
    char *output;
    char *steps;
    char *error;
    Expr *result_ast;
} EculidResult;

EculidResult ec_evaluate(EculidSession *session, const char *input);
void         ec_result_free(EculidResult *r);

#endif /* ECULID_H */
