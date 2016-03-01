/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */

#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "opal/runtime/opal_osv_support.h"

/* 
 * Return 1 if inside OSv, 0 otherwise.
 * */
int opal_is_osv() {
    /* osv_execve is imported as weak symbol, it is NULL if not present */
    if(osv_execve) {
        return true;
    }
    else {
        return false;
    }
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



