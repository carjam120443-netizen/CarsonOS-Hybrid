#ifndef CARSON_INIT_H
#define CARSON_INIT_H

typedef void (*carson_init_log_fn)(const char *message);

/*
 * CarsonInit currently runs as a kernel-integrated bootstrap coordinator.
 * It is deliberately not advertised as a ring-3 process until process
 * isolation, executable loading, and system calls exist.
 */
void carson_init_begin(carson_init_log_fn logger);
void carson_init_report(const char *component, int started);
void carson_init_finish(void);

#endif
