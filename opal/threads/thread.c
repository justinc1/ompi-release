/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2010      Cisco Systems, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal_config.h"

#include "opal/threads/threads.h"
#include "opal/constants.h"

bool opal_debug_threads = false;

static void opal_thread_construct(opal_thread_t *t);

OBJ_CLASS_INSTANCE(opal_thread_t,
                   opal_object_t,
                   opal_thread_construct, NULL);


/*
 * Constructor
 */
static void opal_thread_construct(opal_thread_t *t)
{
    t->t_run = 0;
#if OPAL_HAVE_POSIX_THREADS
    t->t_handle = (pthread_t) -1;
#endif
}

#if OPAL_HAVE_POSIX_THREADS

/************************************************************************
 * POSIX threads
 ************************************************************************/

#include "opal/runtime/opal_osv_support.h"
/*
 * Util function to pin to CPU a newly spawned pthread on OSv.
 * Actually, there is only one such thread - libevent worker thread.
 */
void* opal_thread_start_wrapper(void *t2) {
    // Now we are executing in new thread, and can pin using pthread_self.
    opal_thread_t *t = (opal_thread_t *) t2;
    fprintf(stderr, "TTRT opal_thread_start_warp... t2->run=%p self=%d\n", t->t_run, pthread_self());
    hack_osv_thread_pin();
    return t->t_run(t);
}

int opal_thread_start(opal_thread_t *t)
{
    int rc;

    if (OPAL_ENABLE_DEBUG) {
        if (NULL == t->t_run || t->t_handle != (pthread_t) -1) {
            return OPAL_ERR_BAD_PARAM;
        }
    }

    rc = pthread_create(&t->t_handle, NULL, opal_thread_start_wrapper, t);

    return (rc == 0) ? OPAL_SUCCESS : OPAL_ERROR;
}


int opal_thread_join(opal_thread_t *t, void **thr_return)
{
    int rc = pthread_join(t->t_handle, thr_return);
    t->t_handle = (pthread_t) -1;
    return (rc == 0) ? OPAL_SUCCESS : OPAL_ERROR;
}


bool opal_thread_self_compare(opal_thread_t *t)
{
    return t->t_handle == pthread_self();
}


opal_thread_t *opal_thread_get_self(void)
{
    opal_thread_t *t = OBJ_NEW(opal_thread_t);
    t->t_handle = pthread_self();
    return t;
}

void opal_thread_kill(opal_thread_t *t, int sig)
{
    pthread_kill(t->t_handle, sig);
}


#else

/************************************************************************
 * No thread support
 ************************************************************************/

int opal_thread_start(opal_thread_t *t)
{
    return OPAL_ERROR;
}


int opal_thread_join(opal_thread_t *t, void **thr_return)
{
    return OPAL_ERROR;
}


bool opal_thread_self_compare(opal_thread_t *t)
{
    return true;
}

opal_thread_t *opal_thread_get_self(void)
{
    return NULL;
}

void opal_thread_kill(opal_thread_t *t, int sig)
{
}

#endif
