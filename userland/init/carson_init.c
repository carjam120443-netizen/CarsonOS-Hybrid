#include "carson_init.h"

static carson_init_log_fn log_message;
static int initialized;

static void log(const char *message)
{
    if (log_message) log_message(message);
}

void carson_init_begin(carson_init_log_fn logger)
{
    log_message = logger;
    initialized = 1;
    log("\nCarsonInit: bootstrap coordinator starting\n");
    log("CarsonInit: mode = kernel-integrated bootstrap\n");
    log("CarsonInit: user-mode process support is not available yet\n");
}

void carson_init_report(const char *component, int started)
{
    if (!initialized || !component) return;
    log("CarsonInit: ");
    log(started ? "[started] " : "[deferred] ");
    log(component);
    log("\n");
}

void carson_init_finish(void)
{
    if (!initialized) return;
    log("CarsonInit: startup sequence complete\n");
    log("CarsonInit: deferred services are unavailable, not fatal\n");
    initialized = 0;
}
