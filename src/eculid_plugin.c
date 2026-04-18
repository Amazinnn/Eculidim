#include "eculid.h"

static EculidPlugin g_plugins[EC_MAX_PLUGINS];
static int g_plugin_count = 0;

void ec_plugin_init(void) { g_plugin_count = 0; }

int ec_plugin_register(const EculidPlugin *p) {
    if (g_plugin_count >= EC_MAX_PLUGINS) return 0;
    g_plugins[g_plugin_count++] = *p;
    return 1;
}

Expr* ec_plugin_dispatch(const char *op, const char *input, Expr *ast, void **ctx) {
    (void)op; (void)input; (void)ctx;
    /* TODO: 查找注册的插件并分发 */
    (void)g_plugins;
    return ast;
}

void ec_plugin_list_all(void) {
    for (int i = 0; i < g_plugin_count; i++)
        printf("%s: %s\n", g_plugins[i].name,
               g_plugins[i].description ? g_plugins[i].description : "");
}

int ec_plugin_count(void) { return g_plugin_count; }

void ec_plugin_free_all(void) { g_plugin_count = 0; }
