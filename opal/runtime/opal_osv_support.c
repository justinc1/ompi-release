/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "opal/runtime/opal_osv_support.h"

/* 
 * Return 1 if inside OSv, 0 otherwise.
 * */
int opal_is_osv() {
    static bool first_call=true, is_osv;
    if(first_call) {
        char *osv_ver;
        osv_ver = getenv("OSV_VERSION");
        is_osv = osv_ver != NULL;
        first_call = false;
    }
    return is_osv;
}

/**
 * Replacement for getpid().
 * In Linux, return usual process ID.
 * In OSv, return thread ID instead of process ID.
 */
pid_t opal_getpid()
{
    pid_t id;
    if(opal_is_osv()) {
        id = syscall(__NR_gettid);
    }
    else {
        id = getpid();
    }
    return id;
}

long osv_waittid(long tid, int *status, int options) {
    return syscall(__NR_osv_waittid, tid, status, options);
}

long osv_execve(const char *path, char *const argv[], char *const envp[], long *thread_id, int notification_fd) {
    return syscall(__NR_osv_execve, path, argv, envp, thread_id, notification_fd);
}

