#ifndef OPAL_OSV_SUPPORT_H
#define OPAL_OSV_SUPPORT_H

#include "opal_config.h"

// Replacement for waitpid.
long osv_waittid(long tid, int *status, int options) __attribute__((weak));
// Replacement for fork+exec
long osv_execve(const char *path, char *const argv[], char *const envp[], long *thread_id, int notification_fd) __attribute__((weak));

void hack_osv_thread_pin();

BEGIN_C_DECLS

OPAL_DECLSPEC int opal_is_osv();
OPAL_DECLSPEC pid_t opal_getpid();

END_C_DECLS

#endif
