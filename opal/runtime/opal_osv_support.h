#ifndef OPAL_OSV_SUPPORT_H
#define OPAL_OSV_SUPPORT_H

#include "opal_config.h"

struct http_client_t {
    char *host;
    int port;
    int sockfd;
};
typedef struct http_client_t http_client_t;

// Replacement for waitpid.
long osv_waittid(long tid, int *status, int options) __attribute__((weak));
// Replacement for fork+exec
long osv_execve(const char *path, char *const argv[], char *const envp[], long *thread_id, int notification_fd) __attribute__((weak));

int osv_get_all_app_threads(pid_t tid, pid_t** tid_arr, size_t *len) __attribute__((weak));

BEGIN_C_DECLS

OPAL_DECLSPEC int opal_is_osv();
OPAL_DECLSPEC pid_t opal_getpid();
OPAL_DECLSPEC int opal_osvrest_run(char *host, int port, char **argv);

END_C_DECLS

#endif
