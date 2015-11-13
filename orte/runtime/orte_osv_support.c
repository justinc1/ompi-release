/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */

#include <stdlib.h>

/* 
 * Return 1 if inside OSv, 0 otherwise.
 * */
int orte_is_osv() {
    char *osv_ver;
    osv_ver = getenv("OSV_VERSION");
    return osv_ver != NULL;
}

