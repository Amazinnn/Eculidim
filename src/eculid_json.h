/* eculid_json.h — JSON output engine */
#ifndef ECULID_JSON_H
#define ECULID_JSON_H

#include "eculid_ast.h"
#include "eculid_session.h"

/* ECErrorCode — duplicated from eculid.h so eculid_json.c can use it */
#ifndef EC_ERR_NONE
typedef enum {
    EC_ERR_NONE=0, EC_ERR_PARSE, EC_ERR_UNDEF_VAR, EC_ERR_UNSUP_CMD,
    EC_ERR_DIV_ZERO, EC_ERR_NO_INT, EC_ERR_NO_SOLVE, EC_ERR_DEF_SYNTAX,
    EC_ERR_MODE, EC_ERR_IO, EC_ERR_NO_STEPS, EC_ERR_NO_CONV, EC_ERR_BAD_ARG,
    EC_ERR_INT_LOOP
} ECErrorCode;
#endif

/*============================================================
 * JSON Output Engine
 *============================================================*/

typedef struct {
    char *result;
    char **steps;
    int n_steps;
    int step_capacity;
    ECErrorCode error_code;
    char *error_message;
} ECJSONOutput;

ECJSONOutput* ec_json_new(void);
void ec_json_reset(ECJSONOutput *o);
void ec_json_set_result(ECJSONOutput *o, const char *latex);
void ec_json_add_step(ECJSONOutput *o, const char *latex, const char *rule);
void ec_json_set_error(ECJSONOutput *o, ECErrorCode code, const char *msg);
char* ec_json_render(const ECJSONOutput *o);
void ec_json_free(ECJSONOutput *o);

#endif
