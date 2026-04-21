#include "eculid_json.h"
#include "eculid_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================
 * JSON Output Engine — implementation
 *============================================================*/

ECJSONOutput* ec_json_new(void) {
    ECJSONOutput *o = ec_malloc(sizeof(ECJSONOutput));
    o->result = NULL;
    o->steps = ec_malloc(16 * sizeof(char*));
    o->n_steps = 0;
    o->step_capacity = 16;
    o->error_code = EC_ERR_NONE;
    o->error_message = NULL;
    return o;
}

void ec_json_reset(ECJSONOutput *o) {
    if (!o) return;
    if (o->result) { ec_free(o->result); o->result = NULL; }
    for (int i = 0; i < o->n_steps; i++) {
        ec_free(o->steps[i]);
    }
    o->n_steps = 0;
    if (o->error_message) { ec_free(o->error_message); o->error_message = NULL; }
    o->error_code = EC_ERR_NONE;
}

void ec_json_set_result(ECJSONOutput *o, const char *latex) {
    if (!o) return;
    if (o->result) { ec_free(o->result); o->result = NULL; }
    if (latex) o->result = ec_strdup(latex);
}

void ec_json_add_step(ECJSONOutput *o, const char *latex, const char *rule) {
    if (!o) return;
    if (o->n_steps >= o->step_capacity) {
        o->step_capacity *= 2;
        o->steps = ec_realloc(o->steps, o->step_capacity * sizeof(char*));
    }
    /* format: "[N] latex (rule)" */
    char buf[8192];
    snprintf(buf, sizeof(buf), "[%d] %s (%s)", o->n_steps + 1,
             latex ? latex : "", rule ? rule : "");
    o->steps[o->n_steps] = ec_strdup(buf);
    o->n_steps++;
}

void ec_json_set_error(ECJSONOutput *o, ECErrorCode code, const char *msg) {
    if (!o) return;
    if (o->error_message) { ec_free(o->error_message); o->error_message = NULL; }
    o->error_code = code;
    if (msg) o->error_message = ec_strdup(msg);
}

/* JSON-escape a string into a caller-supplied buffer.
   Caller must ensure buf has enough space. */
static void json_escape_into(const char *s, char *buf, size_t cap) {
    size_t j = 0;
    for (int i = 0; s[i] && j < cap - 1; i++) {
        switch (s[i]) {
            case '\\': case '"':
                if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = s[i]; }
                break;
            case '\n':
                if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = 'n'; }
                break;
            case '\r':
                if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = 'r'; }
                break;
            case '\t':
                if (j + 2 < cap) { buf[j++] = '\\'; buf[j++] = 't'; }
                break;
            default:
                buf[j++] = s[i];
                break;
        }
    }
    buf[j] = '\0';
}

/* Return the EC error code as a string like "EC001" */
static const char* ec_error_code_str(ECErrorCode code) {
    static const char *names[] = {
        [EC_ERR_NONE]      = "EC000",
        [EC_ERR_PARSE]     = "EC001",
        [EC_ERR_UNDEF_VAR] = "EC002",
        [EC_ERR_UNSUP_CMD] = "EC003",
        [EC_ERR_DIV_ZERO]  = "EC004",
        [EC_ERR_NO_INT]    = "EC005",
        [EC_ERR_NO_SOLVE]  = "EC006",
        [EC_ERR_DEF_SYNTAX]= "EC007",
        [EC_ERR_MODE]      = "EC008",
        [EC_ERR_IO]        = "EC009",
        [EC_ERR_NO_STEPS]  = "EC010",
        [EC_ERR_NO_CONV]   = "EC011",
        [EC_ERR_BAD_ARG]   = "EC012",
        [EC_ERR_INT_LOOP]  = "EC013",
    };
    if (code >= 0 && code <= EC_ERR_INT_LOOP) return names[code];
    return "EC000";
}

char* ec_json_render(const ECJSONOutput *o) {
    static char buf[8192];
    buf[0] = '\0';
    if (!o) {
        snprintf(buf, sizeof(buf),
                 "{\n  \"result\": null,\n  \"steps\": null,\n  \"error\": {\"code\": \"EC000\", \"message\": \"null output\"}\n}\n");
        return buf;
    }

    char *p = buf;
    size_t rem = sizeof(buf);
    int n;

    n = snprintf(p, rem, "{\n");
    p += n; rem -= (n > 0 ? n : 0);
    if (rem <= 1) return buf;

    /* result field */
    if (o->result) {
        char esc[4096];
        json_escape_into(o->result, esc, sizeof(esc));
        n = snprintf(p, rem, "  \"result\": \"%s\",\n", esc);
    } else {
        n = snprintf(p, rem, "  \"result\": null,\n");
    }
    p += n; rem -= (n > 0 ? n : 0);
    if (rem <= 1) return buf;

    /* steps field */
    if (o->n_steps > 0) {
        n = snprintf(p, rem, "  \"steps\": [\n");
        p += n; rem -= (n > 0 ? n : 0);
        if (rem <= 1) return buf;
        for (int i = 0; i < o->n_steps; i++) {
            char esc[4096];
            json_escape_into(o->steps[i], esc, sizeof(esc));
            n = snprintf(p, rem, "    \"%s\"%s\n", esc,
                         i < o->n_steps - 1 ? "," : "");
            p += n; rem -= (n > 0 ? n : 0);
            if (rem <= 1) return buf;
        }
        n = snprintf(p, rem, "  ],\n");
        p += n; rem -= (n > 0 ? n : 0);
        if (rem <= 1) return buf;
    } else {
        n = snprintf(p, rem, "  \"steps\": null,\n");
        p += n; rem -= (n > 0 ? n : 0);
        if (rem <= 1) return buf;
    }

    /* error field */
    if (o->error_code != EC_ERR_NONE && o->error_message) {
        char esc[4096];
        json_escape_into(o->error_message, esc, sizeof(esc));
        n = snprintf(p, rem, "  \"error\": {\"code\": \"%s\", \"message\": \"%s\"}\n",
                     ec_error_code_str(o->error_code), esc);
    } else {
        n = snprintf(p, rem, "  \"error\": null\n");
    }
    p += n; rem -= (n > 0 ? n : 0);
    if (rem <= 1) return buf;

    n = snprintf(p, rem, "}\n");
    return buf;
}

void ec_json_free(ECJSONOutput *o) {
    if (!o) return;
    if (o->result) { ec_free(o->result); o->result = NULL; }
    for (int i = 0; i < o->n_steps; i++) {
        ec_free(o->steps[i]);
    }
    ec_free(o->steps);
    if (o->error_message) { ec_free(o->error_message); o->error_message = NULL; }
    ec_free(o);
}
