#ifndef OPAL_OSV_SUPPORT_H
#define OPAL_OSV_SUPPORT_H

#include "opal_config.h"

/* Extra OSv specific syscalls */
#define __NR_osv_execve  1000
#define __NR_osv_waittid 1001
long osv_waittid(long tid, int *status, int options);
// Replacement for fork+exec
long osv_execve(const char *path, char *const argv[], char *const envp[], long *thread_id, int notification_fd);

BEGIN_C_DECLS

OPAL_DECLSPEC int opal_is_osv();

END_C_DECLS

#endif
