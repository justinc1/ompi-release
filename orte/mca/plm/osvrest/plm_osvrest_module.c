/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2007 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2006 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2007 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2012 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2008-2009 Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2011      IBM Corporation.  All rights reserved.
 * Copyright (c) 2014-2015 Intel Corporation.  All rights reserved.
 * Copyright (c) 2015      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <stdlib.h>

#include "opal/mca/installdirs/installdirs.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/mca/event/event.h"
#include "opal/util/argv.h"
#include "opal/util/opal_environ.h"
#include "opal/util/basename.h"
#include "opal/util/path.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/runtime/opal_osv_support.h"

#include "orte/util/show_help.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/util/proc_info.h"

#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/ess/ess.h"
#include "orte/mca/ess/base/base.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rmaps/rmaps.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/rml/base/rml_contact.h"
#include "orte/mca/state/state.h"

#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/base/base.h"
#include "orte/mca/plm/base/plm_private.h"
#include "orte/mca/plm/osvrest/plm_osvrest.h"

static int osvrest_init(void);
static int osvrest_launch(orte_job_t *jdata);
static int remote_spawn(opal_buffer_t *launch);
static int osvrest_terminate_orteds(void);
static int osvrest_finalize(void);

orte_plm_base_module_t orte_plm_osvrest_module = {
    osvrest_init,
    orte_plm_base_set_hnp_name,
    osvrest_launch,
    remote_spawn,
    orte_plm_base_orted_terminate_job,
    osvrest_terminate_orteds,
    orte_plm_base_orted_kill_local_procs,
    orte_plm_base_orted_signal_local_procs,
    osvrest_finalize
};

typedef struct {
    opal_list_item_t super;
    int argc;
    char **argv;
    orte_proc_t *daemon;
} orte_plm_osvrest_caddy_t;
static void caddy_const(orte_plm_osvrest_caddy_t *ptr)
{
    ptr->argv = NULL;
    ptr->daemon = NULL;
}
static void caddy_dest(orte_plm_osvrest_caddy_t *ptr)
{
    if (NULL != ptr->argv) {
        opal_argv_free(ptr->argv);
    }
    if (NULL != ptr->daemon) {
        OBJ_RELEASE(ptr->daemon);
    }
}
OBJ_CLASS_INSTANCE(orte_plm_osvrest_caddy_t,
                   opal_list_item_t,
                   caddy_const, caddy_dest);

/*
 * Local functions
 */
static int launch_agent_setup(const char *agent, char *path);
static void launch_daemons(int fd, short args, void *cbdata);
static void process_launch_list(int fd, short args, void *cbdata);
static void osvrest_child(int argc, char **argv);

/* local global storage */
static char **osvrest_agent_argv=NULL;
static int num_in_progress=0;
static opal_list_t launch_list;
static opal_event_t launch_event;

/**
 * Init the module
 */
static int osvrest_init(void)
{
    char *tmp;
    int rc;
    
    /* we were selected, so setup the launch agent */
    /* not using qrsh or llspawn - use MCA-specified agent */
    if (ORTE_SUCCESS != (rc = launch_agent_setup(mca_plm_osvrest_component.agent, NULL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* point to our launch command */
    if (ORTE_SUCCESS != (rc = orte_state.add_job_state(ORTE_JOB_STATE_LAUNCH_DAEMONS,
                                                       launch_daemons, ORTE_SYS_PRI))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* setup the event for metering the launch */
    OBJ_CONSTRUCT(&launch_list, opal_list_t);
    opal_event_set(orte_event_base, &launch_event, -1, 0, process_launch_list, NULL);
    opal_event_set_priority(&launch_event, ORTE_SYS_PRI);
    
    /* start the recvs */
    if (ORTE_SUCCESS != (rc = orte_plm_base_comm_start())) {
        ORTE_ERROR_LOG(rc);
    }
    
    /* we assign daemon nodes at launch */
    orte_plm_globals.daemon_nodes_assigned_at_launch = true;
    
    return rc;
}

/**
 * Callback on daemon exit.
 */
static void osvrest_wait_daemon(pid_t pid, int status, void* cbdata) /* OSv check */
{
    orte_job_t *jdata;
    orte_plm_osvrest_caddy_t *caddy=(orte_plm_osvrest_caddy_t*)cbdata;
    orte_proc_t *daemon=caddy->daemon;
    
    if (orte_orteds_term_ordered || orte_abnormal_term_ordered) {
        /* ignore any such report - it will occur if we left the
         * session attached, e.g., while debugging
         */
        OBJ_RELEASE(caddy);
        return;
    }

    if (! WIFEXITED(status) || ! WEXITSTATUS(status) == 0) { /* if abnormal exit */
        /* if we are not the HNP, send a message to the HNP alerting it
         * to the failure
         */
        if (!ORTE_PROC_IS_HNP) {
            opal_buffer_t *buf;
            OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                                 "%s daemon %d failed with status %d",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 (int)daemon->name.vpid, WEXITSTATUS(status)));
            buf = OBJ_NEW(opal_buffer_t);
            opal_dss.pack(buf, &(daemon->name.vpid), 1, ORTE_VPID);
            opal_dss.pack(buf, &status, 1, OPAL_INT);
            orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buf,
                                    ORTE_RML_TAG_REPORT_REMOTE_LAUNCH,
                                    orte_rml_send_callback, NULL);
            /* note that this daemon failed */
            daemon->state = ORTE_PROC_STATE_FAILED_TO_START;
        } else {
            jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid);
            
            OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                                 "%s daemon %d failed with status %d",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 (int)daemon->name.vpid, WEXITSTATUS(status)));
            /* set the exit status */
            ORTE_UPDATE_EXIT_STATUS(WEXITSTATUS(status));
            /* note that this daemon failed */
            daemon->state = ORTE_PROC_STATE_FAILED_TO_START;
            /* increment the #daemons terminated so we will exit properly */
            jdata->num_terminated++;
            /* remove it from the routing table to ensure num_routes
             * returns the correct value
             */
            orte_routed.route_lost(&daemon->name);
            /* report that the daemon has failed so we can exit */
            ORTE_ACTIVATE_PROC_STATE(&daemon->name, ORTE_PROC_STATE_FAILED_TO_START);
        }
    }
    
    /* release any delay */
    --num_in_progress;
    if (num_in_progress < mca_plm_osvrest_component.num_concurrent) {
        /* trigger continuation of the launch */
        opal_event_active(&launch_event, EV_WRITE, 1);
    }
    /* cleanup */
    OBJ_RELEASE(caddy);
}

static int setup_launch(int *argcptr, char ***argvptr,
                        char *nodename,
                        int *node_name_index1,
                        int *proc_vpid_index, char *prefix_dir)
{
    int argc;
    char **argv;
    char *param, *value;
    int orted_argc;
    char **orted_argv;
    char *orted_cmd, *orted_prefix, *final_cmd;
    int orted_index;
    int rc;
    int i, j;
    bool found;
    char *lib_base=NULL, *bin_base=NULL;
    char *opal_prefix = getenv("OPAL_PREFIX");
    char* full_orted_cmd = NULL;

    /*
     * Build argv array
     */
    argv = opal_argv_copy(osvrest_agent_argv);
    argc = opal_argv_count(osvrest_agent_argv);
    *node_name_index1 = argc;
    opal_argv_append(&argc, &argv, "<template>");
    
    /* now get the orted cmd - as specified by user - into our tmp array.
     * The function returns the location where the actual orted command is
     * located - usually in the final spot, but someone could
     * have added options. For example, it should be legal for them to use
     * "orted --debug-devel" so they get debug output from the orteds, but
     * not from mpirun. Also, they may have a customized version of orted
     * that takes arguments in addition to the std ones we already support
     */
    orted_argc = 0;
    orted_argv = NULL;
    orted_index = orte_plm_base_setup_orted_cmd(&orted_argc, &orted_argv);
    
    /* look at the returned orted cmd argv to check several cases:
     *
     * - only "orted" was given. This is the default and thus most common
     *   case. In this situation, there is nothing we need to do
     *
     * - something was given that doesn't include "orted" - i.e., someone
     *   has substituted their own daemon. There isn't anything we can
     *   do here, so we want to avoid adding prefixes to the cmd
     *
     * - something was given that precedes "orted". For example, someone
     *   may have specified "valgrind [options] orted". In this case, we
     *   need to separate out that "orted_prefix" section so it can be
     *   treated separately below
     *
     * - something was given that follows "orted". An example was given above.
     *   In this case, we need to construct the effective "orted_cmd" so it
     *   can be treated properly below
     *
     * Obviously, the latter two cases can be combined - just to make it
     * even more interesting! Gotta love rsh/ssh...
     */
    if (0 == orted_index) {
        /* single word cmd - this is the default scenario, but there could
         * be options specified so we need to account for that possibility.
         * However, we don't need/want a prefix as nothing precedes the orted
         * cmd itself
         */
        orted_cmd = opal_argv_join(orted_argv, ' ');
        orted_prefix = NULL;
    } else {
        /* okay, so the "orted" cmd is somewhere in this array, with
         * something preceding it and perhaps things following it.
         */
        orted_prefix = opal_argv_join_range(orted_argv, 0, orted_index, ' ');
        orted_cmd = opal_argv_join_range(orted_argv, orted_index, opal_argv_count(orted_argv), ' ');
    }
    opal_argv_free(orted_argv);  /* done with this */

    /* no prefix directory, so just aggregate the result */
    asprintf(&final_cmd, "%s %s",
             (orted_prefix != NULL ? orted_prefix : ""),
             (full_orted_cmd != NULL ? full_orted_cmd : ""));
    if (NULL != full_orted_cmd) free(full_orted_cmd);

    /* now add the final cmd to the argv array */
    opal_argv_append(&argc, &argv, final_cmd);
    free(final_cmd);  /* done with this */
    if (NULL != orted_prefix) free(orted_prefix);
    if (NULL != orted_cmd) free(orted_cmd);
    
    /* if we are not tree launching or debugging, tell the daemon
     * to daemonize so we can launch the next group
     */
    if (mca_plm_osvrest_component.no_tree_spawn &&
        !orte_debug_flag &&
        !orte_debug_daemons_flag &&
        !orte_debug_daemons_file_flag &&
        !orte_leave_session_attached) {
        opal_argv_append(&argc, &argv, "--daemonize");
    }
    
    /*
     * Add the basic arguments to the orted command line, including
     * all debug options
     */
    orte_plm_base_orted_append_basic_args(&argc, &argv,
                                          "env",
                                          proc_vpid_index,
                                          NULL);
    
    /* ensure that only the osvrest plm is selected on the remote daemon */
    opal_argv_append_nosize(&argv, "-mca");
    opal_argv_append_nosize(&argv, "plm");
    opal_argv_append_nosize(&argv, "osvrest");
    
    /* unless told otherwise... */
    if (mca_plm_osvrest_component.pass_environ_mca_params) {
        /* now check our local environment for MCA params - add them
         * only if they aren't already present
         */
        for (i = 0; NULL != environ[i]; ++i) {
            if (0 == strncmp("OMPI_MCA_mca_base_env_list", environ[i],
                             strlen("OMPI_MCA_mca_base_env_list"))) {
                /* ignore this one */
                continue;
            }
            if (0 == strncmp("OMPI_MCA_", environ[i], 9)) {
                /* check for duplicate in app->env - this
                 * would have been placed there by the
                 * cmd line processor. By convention, we
                 * always let the cmd line override the
                 * environment
                 */
                param = strdup(&environ[i][9]);
                value = strchr(param, '=');
                *value = '\0';
                value++;
                found = false;
                /* see if this param exists on the cmd line */
                for (j=0; NULL != argv[j]; j++) {
                    if (0 == strcmp(param, argv[j])) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* add it */
                    opal_argv_append(&argc, &argv, "-mca");
                    opal_argv_append(&argc, &argv, param);
                    opal_argv_append(&argc, &argv, value);
                }
                free(param);
            }
        }
    }

    /* protect the params */
    mca_base_cmd_line_wrap_args(argv);

    /* tell the daemon we are in a tree spawn */
    opal_argv_append(&argc, &argv, "--tree-spawn");

    if (0 < opal_output_get_verbosity(orte_plm_base_framework.framework_output)) {
        param = opal_argv_join(argv, ' ');
        opal_output(orte_plm_base_framework.framework_output,
                    "%s plm:osvrest: final template argv:\n\t%s",
                    ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                    (NULL == param) ? "NULL" : param);
        if (NULL != param) free(param);
    }
    
    /* all done */
    *argcptr = argc;
    *argvptr = argv;
    return ORTE_SUCCESS;
}

/*
 * launch a set of daemons from a remote daemon
 */
static int remote_spawn(opal_buffer_t *launch)
{
    opal_list_item_t *item;
    int node_name_index1;
    int proc_vpid_index;
    char **argv = NULL;
    char *prefix, *hostname, *var;
    int argc;
    int rc=ORTE_SUCCESS;
    bool failed_launch = true;
    orte_std_cntr_t n;
    opal_byte_object_t *bo;
    orte_process_name_t target;
    orte_plm_osvrest_caddy_t *caddy;
    orte_job_t *daemons;
    orte_grpcomm_collective_t coll;

    OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                         "%s plm:osvrest: remote spawn called",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* if we hit any errors, tell the HNP it was us */
    target.vpid = ORTE_PROC_MY_NAME->vpid;

    /* extract the prefix from the launch buffer */
    n = 1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(launch, &prefix, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }            
    
    /* extract the byte object holding the nidmap */
    n=1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(launch, &bo, &n, OPAL_BYTE_OBJECT))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* update our nidmap - this will free data in the byte object */
    if (ORTE_SUCCESS != (rc = orte_util_decode_daemon_nodemap(bo))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    
    /* ensure the routing plan is updated */
    orte_routed.update_routing_plan();
    
    /* get the updated routing list */
    OBJ_CONSTRUCT(&coll, orte_grpcomm_collective_t);
    orte_routed.get_routing_list(ORTE_GRPCOMM_XCAST, &coll);
    
    /* if I have no children, just return */
    if (0 == opal_list_get_size(&coll.targets)) {
        OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                             "%s plm:osvrest: remote spawn - have no children!",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        failed_launch = false;
        rc = ORTE_SUCCESS;
        OBJ_DESTRUCT(&coll);
        goto cleanup;
    }
    
    /* setup the launch */
    if (ORTE_SUCCESS != (rc = setup_launch(&argc, &argv, orte_process_info.nodename, &node_name_index1,
                                           &proc_vpid_index, prefix))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&coll);
        goto cleanup;
    }

    /* get the daemon job object */
    if (NULL == (daemons = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        rc = ORTE_ERR_NOT_FOUND;
        OBJ_DESTRUCT(&coll);
        goto cleanup;
    }
    
    target.jobid = ORTE_PROC_MY_NAME->jobid;
    for (item = opal_list_get_first(&coll.targets);
         item != opal_list_get_end(&coll.targets);
         item = opal_list_get_next(item)) {
        orte_namelist_t *child = (orte_namelist_t*)item;
        target.vpid = child->name.vpid;
        
        /* get the host where this daemon resides */
        if (NULL == (hostname = orte_get_proc_hostname(&target))) {
            opal_output(0, "%s unable to get hostname for daemon %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), ORTE_VPID_PRINT(child->name.vpid));
            rc = ORTE_ERR_NOT_FOUND;
            OBJ_DESTRUCT(&coll);
            goto cleanup;
        }
        
        free(argv[node_name_index1]);
        argv[node_name_index1] = strdup(hostname);
        
        /* pass the vpid */
        rc = orte_util_convert_vpid_to_string(&var, target.vpid);
        if (ORTE_SUCCESS != rc) {
            opal_output(0, "orte_plm_osvrest: unable to get daemon vpid as string");
            exit(-1);
        }
        free(argv[proc_vpid_index]);
        argv[proc_vpid_index] = strdup(var);
        free(var);
        
        /* we are in an event, so no need to protect the list */
        caddy = OBJ_NEW(orte_plm_osvrest_caddy_t);
        caddy->argc = argc;
        caddy->argv = opal_argv_copy(argv);
        /* fake a proc structure for the new daemon - will be released
         * upon startup
         */
        caddy->daemon = OBJ_NEW(orte_proc_t);
        caddy->daemon->name.jobid = ORTE_PROC_MY_NAME->jobid;
        caddy->daemon->name.vpid = target.vpid;
        opal_list_append(&launch_list, &caddy->super);
    }
    OBJ_DESTRUCT(&coll);
    
    /* trigger the event to start processing the launch list */
    OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                         "%s plm:osvrest: activating launch event",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    opal_event_active(&launch_event, EV_WRITE, 1);

    /* declare the launch a success */
    failed_launch = false;
    
cleanup:    
    if (NULL != argv) {
        opal_argv_free(argv);
    }
    
    /* check for failed launch */
    if (failed_launch) {
        /* report cannot launch this daemon to HNP */
        opal_buffer_t *buf;
        buf = OBJ_NEW(opal_buffer_t);
        opal_dss.pack(buf, &target.vpid, 1, ORTE_VPID);
        opal_dss.pack(buf, &rc, 1, OPAL_INT);
        orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buf,
                                ORTE_RML_TAG_REPORT_REMOTE_LAUNCH,
                                orte_rml_send_callback, NULL);
    }
    
    return rc;
}

/*
 * Launch a daemon (bootproxy) on each node. The daemon will be responsible
 * for launching the application.
 */

static int osvrest_launch(orte_job_t *jdata)
{
    if (ORTE_JOB_CONTROL_RESTART & jdata->controls) {
        /* this is a restart situation - skip to the mapping stage */
        ORTE_ACTIVATE_JOB_STATE(jdata, ORTE_JOB_STATE_MAP);
    } else {
        /* new job - set it up */
        ORTE_ACTIVATE_JOB_STATE(jdata, ORTE_JOB_STATE_INIT);
    }
    return ORTE_SUCCESS;
}

static void process_launch_list(int fd, short args, void *cbdata)
{
    opal_list_item_t *item;
    pid_t pid;
    orte_plm_osvrest_caddy_t *caddy;
    
    while (num_in_progress < mca_plm_osvrest_component.num_concurrent) {
        item = opal_list_remove_first(&launch_list);
        if (NULL == item) {
            /* we are done */
            break;
        }
        caddy = (orte_plm_osvrest_caddy_t*)item;
        
        /* do the launch via REST - this will exit if it fails */
        osvrest_child(caddy->argc, caddy->argv);

        /* indicate this daemon has been launched */
        caddy->daemon->state = ORTE_PROC_STATE_RUNNING;
        /* record the pid of the ssh fork */
        /*
        For OSv, separate thread should be started, and thread ID should be saved.
        For now, just set it to -1 to prevent using waitpid.
        */
        pid = -1;
        caddy->daemon->pid = pid;
        
        OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                             "%s plm:osvrest: recording launch of daemon %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&(caddy->daemon->name))));

        /* setup callback on sigchild - wait until setup above is complete
         * as the callback can occur in the call to orte_wait_cb
         */
        orte_wait_cb(pid, osvrest_wait_daemon, (void*)caddy);
        num_in_progress++;

    }
}

static void launch_daemons(int fd, short args, void *cbdata)
{
    orte_job_map_t *map = NULL;
    int node_name_index1;
    int proc_vpid_index;
    char **argv = NULL;
    char *prefix_dir=NULL, *var;
    int argc;
    int rc;
    orte_app_context_t *app;
    orte_node_t *node, *nd;
    orte_std_cntr_t nnode;
    opal_list_item_t *item;
    orte_job_t *daemons;
    orte_state_caddy_t *state = (orte_state_caddy_t*)cbdata;
    orte_plm_osvrest_caddy_t *caddy;
    orte_grpcomm_collective_t coll;

    /* if we are launching debugger daemons, then just go
     * do it - no new daemons will be launched
     */
    if (ORTE_JOB_CONTROL_DEBUGGER_DAEMON & state->jdata->controls) {
        state->jdata->state = ORTE_JOB_STATE_DAEMONS_LAUNCHED;
        ORTE_ACTIVATE_JOB_STATE(state->jdata, ORTE_JOB_STATE_DAEMONS_REPORTED);
        OBJ_RELEASE(state);
        return;
    }
    
    /* setup the virtual machine */
    daemons = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid);
    if (ORTE_SUCCESS != (rc = orte_plm_base_setup_virtual_machine(state->jdata))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    
    /* if we don't want to launch, then don't attempt to
     * launch the daemons - the user really wants to just
     * look at the proposed process map
     */
    if (orte_do_not_launch) {
        /* set the state to indicate the daemons reported - this
         * will trigger the daemons_reported event and cause the
         * job to move to the following step
         */
        state->jdata->state = ORTE_JOB_STATE_DAEMONS_LAUNCHED;
        ORTE_ACTIVATE_JOB_STATE(state->jdata, ORTE_JOB_STATE_DAEMONS_REPORTED);
        OBJ_RELEASE(state);
        return;
    }
    
    /* Get the map for this job */
    if (NULL == (map = daemons->map)) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        rc = ORTE_ERR_NOT_FOUND;
        goto cleanup;
    }
    
    if (0 == map->num_new_daemons) {
        /* set the state to indicate the daemons reported - this
         * will trigger the daemons_reported event and cause the
         * job to move to the following step
         */
        state->jdata->state = ORTE_JOB_STATE_DAEMONS_LAUNCHED;
        ORTE_ACTIVATE_JOB_STATE(state->jdata, ORTE_JOB_STATE_DAEMONS_REPORTED);
        OBJ_RELEASE(state);
        return;
    }
    
    OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                         "%s plm:osvrest: launching vm",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    if ((0 < opal_output_get_verbosity(orte_plm_base_framework.framework_output) ||
         orte_leave_session_attached) &&
        mca_plm_osvrest_component.num_concurrent < map->num_new_daemons) {
        /**
         * If we are in '--debug-daemons' we keep the ssh connection 
         * alive for the span of the run. If we use this option 
         * AND we launch on more than "num_concurrent" machines
         * then we will deadlock. No connections are terminated 
         * until the job is complete, no job is started
         * since all the orteds are waiting for all the others
         * to come online, and the others ore not launched because
         * we are waiting on those that have started to terminate
         * their ssh tunnels. :(
         * As we cannot run in this situation, pretty print the error
         * and return an error code.
         */
        orte_show_help("help-plm-rsh.txt", "deadlock-params",
                       true, mca_plm_osvrest_component.num_concurrent, map->num_new_daemons);
        ORTE_ERROR_LOG(ORTE_ERR_FATAL);
        OBJ_RELEASE(state);
        return;
    }
    
    /*
     * After a discussion between Ralph & Jeff, we concluded that we
     * really are handling the prefix dir option incorrectly. It currently
     * is associated with an app_context, yet it really refers to the
     * location where OpenRTE/Open MPI is installed on a NODE. Fixing
     * this right now would involve significant change to orterun as well
     * as elsewhere, so we will intentionally leave this incorrect at this
     * point. The error, however, is identical to that seen in all prior
     * releases of OpenRTE/Open MPI, so our behavior is no worse than before.
     *
     * A note to fix this, along with ideas on how to do so, has been filed
     * on the project's Trac system under "feature enhancement".
     *
     * For now, default to the prefix_dir provided in the first app_context.
     * Since there always MUST be at least one app_context, we are safe in
     * doing this.
     */
    app = (orte_app_context_t*)opal_pointer_array_get_item(state->jdata->apps, 0);
    prefix_dir = app->prefix_dir;
    /* we also need at least one node name so we can check what shell is
     * being used, if we have to
     */
    node = NULL;
    for (nnode = 0; nnode < map->nodes->size; nnode++) {
        if (NULL != (nd = (orte_node_t*)opal_pointer_array_get_item(map->nodes, nnode))) {
            node = nd;
            /* if the node is me, then we continue - we would
             * prefer to find some other node so we can tell what the remote
             * shell is, if necessary
             */
            if (0 != strcmp(node->name, orte_process_info.nodename)) {
                break;
            }
        }
    }
            
    /* if we are tree launching, find our children and create the launch cmd */
    if (!mca_plm_osvrest_component.no_tree_spawn) {
        orte_daemon_cmd_flag_t command = ORTE_DAEMON_TREE_SPAWN;
        opal_byte_object_t bo, *boptr;
        orte_job_t *jdatorted;
        
        /* get the tree spawn buffer */
        orte_tree_launch_cmd = OBJ_NEW(opal_buffer_t);
        /* insert the tree_spawn cmd */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(orte_tree_launch_cmd, &command, 1, ORTE_DAEMON_CMD))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(orte_tree_launch_cmd);
            goto cleanup;
        }
        /* pack the prefix since this will be needed by the next wave */
        if (ORTE_SUCCESS != (rc = opal_dss.pack(orte_tree_launch_cmd, &prefix_dir, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(orte_tree_launch_cmd);
            goto cleanup;
        }
        /* construct a nodemap of all daemons we know about */
        if (ORTE_SUCCESS != (rc = orte_util_encode_nodemap(&bo, false))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(orte_tree_launch_cmd);
            goto cleanup;
        }
        /* store it */
        boptr = &bo;
        if (ORTE_SUCCESS != (rc = opal_dss.pack(orte_tree_launch_cmd, &boptr, 1, OPAL_BYTE_OBJECT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(orte_tree_launch_cmd);
            free(bo.bytes);
            goto cleanup;
        }
        /* release the data since it has now been copied into our buffer */
        free(bo.bytes);
        /* get the orted job data object */
        if (NULL == (jdatorted = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            rc = ORTE_ERR_NOT_FOUND;
            goto cleanup;
        }
        
        /* get the updated routing list */
        OBJ_CONSTRUCT(&coll, orte_grpcomm_collective_t);
        orte_routed.get_routing_list(ORTE_GRPCOMM_XCAST, &coll);
    }
    
    /* setup the launch */
    if (ORTE_SUCCESS != (rc = setup_launch(&argc, &argv, node->name, &node_name_index1,
                                           &proc_vpid_index, prefix_dir))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    
    /*
     * Iterate through each of the nodes
     */
    for (nnode=0; nnode < map->nodes->size; nnode++) {
        if (NULL == (node = (orte_node_t*)opal_pointer_array_get_item(map->nodes, nnode))) {
            continue;
        }
        
        /* if we are tree launching, only launch our own children */
        if (!mca_plm_osvrest_component.no_tree_spawn) {
            for (item = opal_list_get_first(&coll.targets);
                 item != opal_list_get_end(&coll.targets);
                 item = opal_list_get_next(item)) {
                orte_namelist_t *child = (orte_namelist_t*)item;
                if (child->name.vpid == node->daemon->name.vpid) {
                    goto launch;
                }
            }
            /* didn't find it - ignore this node */
            OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                                 "%s plm:osvrest:launch daemon %s not a child of mine",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 ORTE_VPID_PRINT(node->daemon->name.vpid)));
            continue;
        }
        
    launch:
        /* if this daemon already exists, don't launch it! */
        if (node->daemon_launched) {
            OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                                 "%s plm:osvrest:launch daemon already exists on node %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 node->name));
            continue;
        }
        
        /* if the node's daemon has not been defined, then we
         * have an error!
         */
        if (NULL == node->daemon) {
            ORTE_ERROR_LOG(ORTE_ERR_FATAL);
            OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                                 "%s plm:osvrest:launch daemon failed to be defined on node %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 node->name));
            continue;
        }
        
        /* setup node name */
        free(argv[node_name_index1]);
        if (NULL != node->username &&
            0 != strlen (node->username)) {
            asprintf (&argv[node_name_index1], "%s@%s",
                      node->username, node->name);
        } else {
            argv[node_name_index1] = strdup(node->name);
        }
        
        /* pass the vpid */
        rc = orte_util_convert_vpid_to_string(&var, node->daemon->name.vpid);
        if (ORTE_SUCCESS != rc) {
            opal_output(0, "orte_plm_osvrest: unable to get daemon vpid as string");
            exit(-1);
        }
        free(argv[proc_vpid_index]);
        argv[proc_vpid_index] = strdup(var);
        free(var);
        
        OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                             "%s plm:osvrest: adding node %s to launch list",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             node->name));
        
        /* we are in an event, so no need to protect the list */
        caddy = OBJ_NEW(orte_plm_osvrest_caddy_t);
        caddy->argc = argc;
        caddy->argv = opal_argv_copy(argv);
        caddy->daemon = node->daemon;
        OBJ_RETAIN(caddy->daemon);
        opal_list_append(&launch_list, &caddy->super);
    }
    
    /* set the job state to indicate the daemons are launched */
    state->jdata->state = ORTE_JOB_STATE_DAEMONS_LAUNCHED;
    
    /* trigger the event to start processing the launch list */
    OPAL_OUTPUT_VERBOSE((1, orte_plm_base_framework.framework_output,
                         "%s plm:osvrest: activating launch event",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    opal_event_active(&launch_event, EV_WRITE, 1);
    
    /* now that we've launched the daemons, let the daemon callback
     * function determine they are all alive and trigger the next stage
     */
    OBJ_RELEASE(state);
    return;
    
cleanup:
    OBJ_RELEASE(state);
    ORTE_FORCED_TERMINATE(ORTE_ERROR_DEFAULT_EXIT_CODE);
}

/**
 * Terminate the orteds for a given job
 */
static int osvrest_terminate_orteds(void)
{
    int rc;
    
    if (ORTE_SUCCESS != (rc = orte_plm_base_orted_exit(ORTE_DAEMON_EXIT_CMD))) {
        ORTE_ERROR_LOG(rc);
    }
    
    return rc;
}

static int osvrest_finalize(void)
{
    int rc, i;
    orte_job_t *jdata;
    orte_proc_t *proc;
    pid_t ret;
    
    /* remove launch event */
    opal_event_del(&launch_event);
    OPAL_LIST_DESTRUCT(&launch_list);
    
    /* cleanup any pending recvs */
    if (ORTE_SUCCESS != (rc = orte_plm_base_comm_stop())) {
        ORTE_ERROR_LOG(rc);
    }

    if ((ORTE_PROC_IS_DAEMON || ORTE_PROC_IS_HNP) && orte_abnormal_term_ordered) {
        /* ensure that any lingering ssh's are gone */
        if (NULL == (jdata = orte_get_job_data_object(ORTE_PROC_MY_NAME->jobid))) {
            return rc;
        }
        for (i=0; i < jdata->procs->size; i++) {
            if (NULL == (proc = opal_pointer_array_get_item(jdata->procs, i))) {
                continue;
            }
            if (0 < proc->pid) {
                /* this is a daemon we started - see if the ssh process still exists */
                /* TODO fix for OSv */
                // ret = waitpid(proc->pid, &proc->exit_code, WNOHANG);
            }
        }
    }
    
    return rc;
}


static int launch_agent_setup(const char *agent, char *path)
{
    /* if no agent was provided, then report not found */
    if (NULL == mca_plm_osvrest_component.agent && NULL == agent) {
        return ORTE_ERR_NOT_FOUND;
    }
    
    /* search for the argv */
    OPAL_OUTPUT_VERBOSE((5, orte_plm_base_framework.framework_output,
                         "%s plm:osvrest_setup on agent %s path %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         (NULL == agent) ? mca_plm_osvrest_component.agent : agent,
                         (NULL == path) ? "NULL" : path));
    // osvrest_agent_argv = orte_plm_osvrest_search(agent, path) ;
    assert(osvrest_agent_argv==NULL);
    osvrest_agent_argv = malloc(2*sizeof(char*));
    osvrest_agent_argv[0] = strdup(agent);  /* As we use abs path, no PATH env var is needed */
    osvrest_agent_argv[1] = NULL;

    /* the caller can append any additional argv's they desire */
    return ORTE_SUCCESS;
}

static void osvrest_child(int argc, char **argv) {
    char* var;
    
    var = opal_argv_join(argv, ' ');
    OPAL_OUTPUT_VERBOSE((10, orte_plm_base_framework.framework_output,
                         "%s plm:osv_rest: orig/linux child command = %s)",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), var));
    /*
    var is like:
        /usr/lib/orted.so 192.168.122.212     PATH=/home/xlab/openmpi-bin/bin:$PATH ; export PATH ; LD_LIBRARY_PATH=/home/xlab/openmpi-bin/lib:$LD_LIBRARY_PATH ; export LD_LIBRARY_PATH ; DYLD_LIBRARY_PATH=/home/xlab/openmpi-bin/lib:$DYLD_LIBRARY_PATH ; export DYLD_LIBRARY_PATH ;   /home/xlab/openmpi-bin/bin/orted -mca orte_debug "1" ...
    We need only
        /home/xlab/openmpi-bin/bin/orted -mca orte_debug "1" ...
    And maybe we should set PATH in OSv VMs too.
    */

    int port = 8000;
    char *host = argv[1];
    argv[1] = argv[0];  // drop IP from argv
    opal_osvrest_run(host, port, argv+1);
    argv[1] = host;
}
