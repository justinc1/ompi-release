/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */

#include "opal/include/opal_config.h" // to define _GNU_SOURCE; must be first include
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "opal/runtime/opal_osv_support.h"
#include <stdio.h>
#include <pthread.h>
#include <sched.h>

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


/**
 * Pin Open MPI thread to vCPU.
 * vCPU number we pin to is based on MPI rank - very simplistic.
 *
 * Note that osv_execve cannot access environ from the caller - getenv("OMPI_COMM_WORLD_RANK")
 * return NULL. So we need to access environ from "userspace".
 */
void hack_osv_thread_pin() {
    if(!opal_is_osv()) {
        //fprintf(stderr, "TTRT hack_osv_thread_pin not on OSv (fyi rank=%d, my_cpu=%d)\n", rank, my_cpu);
        return;
    }

    char* rank_str; /* OMPI_COMM_WORLD_RANK OMPI_COMM_WORLD_LOCAL_RANK OMPI_COMM_WORLD_NODE_RANK */
    // rank_str = getenv("OMPI_COMM_WORLD_LOCAL_RANK");
    rank_str = getenv("OMPI_COMM_WORLD_RANK");
    if(rank_str == NULL) {
        // This is not OMPI worker thread, so maybe it is mpirun program or orted program.
        //fprintf(stderr, "TTRT hack_osv_thread_pin no OMPI_COMM_WORLD_LOCAL_RANK in env\n");
        return;
    }
    int rank = atoi(rank_str);
    char* cpu_count_str = getenv("OSV_CPUS");
    int cpu_count, my_cpu;
    if(cpu_count_str == NULL) {
        //fprintf(stderr, "TTRT hack_osv_thread_pin no OSV_CPUS in env\n");
        return;
    }

    cpu_count = atoi(cpu_count_str);
    my_cpu = (rank+0) % cpu_count;

    int err;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(my_cpu, &cpuset);
    err = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    //fprintf(stderr, "TTRT hack_osv_thread_pin pthread_setaffinity_np to cpu %d, rank=%d, ret = %d\n", my_cpu, rank, err);
}
