/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007-2011 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2008-2009 Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights 
 *                         reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2011      IBM Corporation.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>

#include "orte/runtime/orte_globals.h"

#include "orte/mca/plm/plm.h"
#include "orte/mca/plm/osvrest/plm_osvrest.h"

/*
 * Public string showing the plm ompi_osvrest component version number
 */
const char *mca_plm_osvrest_component_version_string =
  "Open MPI osvrest (OSv REST) plm MCA component version " ORTE_VERSION;


static int osvrest_component_register(void);
static int osvrest_component_open(void);
static int osvrest_component_query(mca_base_module_t **module, int *priority);
static int osvrest_component_close(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

orte_plm_osvrest_component_t mca_plm_osvrest_component = {
    {
    {
        ORTE_PLM_BASE_VERSION_2_0_0,

        /* Component name and version */
        "osvrest",
        ORTE_MAJOR_VERSION,
        ORTE_MINOR_VERSION,
        ORTE_RELEASE_VERSION,

        /* Component open and close functions */
        osvrest_component_open,
        osvrest_component_close,
        osvrest_component_query,
        osvrest_component_register
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    }
    }
};

static int osvrest_component_register(void)
{
    mca_base_component_t *c = &mca_plm_osvrest_component.super.base_version;

    mca_plm_osvrest_component.num_concurrent = 128;
    (void) mca_base_component_var_register (c, "num_concurrent",
                                            "How many plm_osvrest_agent instances to invoke concurrently (must be > 0)",
                                            MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                            OPAL_INFO_LVL_5,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_plm_osvrest_component.num_concurrent);

    /* Default priority is lower that plm rsh */
    mca_plm_osvrest_component.priority = 5;
    (void) mca_base_component_var_register (c, "priority", "Priority of the osvrest plm component",
                                            MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                            OPAL_INFO_LVL_9,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_plm_osvrest_component.priority);

    /* local launch agent */
    mca_plm_osvrest_component.agent = "/usr/lib/orted.so";
    (void) mca_base_component_var_register (c, "agent",
                                              "The command used to launch executables on remote nodes (orted.os)",
                                              MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                              OPAL_INFO_LVL_2,
                                              MCA_BASE_VAR_SCOPE_READONLY,
                                              &mca_plm_osvrest_component.agent);

    mca_plm_osvrest_component.no_tree_spawn = false;
    (void) mca_base_component_var_register (c, "no_tree_spawn",
                                            "If set to true, do not launch via a tree-based topology",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_5,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_plm_osvrest_component.no_tree_spawn);

    mca_plm_osvrest_component.pass_environ_mca_params = true;
    (void) mca_base_component_var_register (c, "pass_environ_mca_params",
                                            "If set to false, do not include mca params from the environment on the orted cmd line",
                                            MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                            OPAL_INFO_LVL_2,
                                            MCA_BASE_VAR_SCOPE_READONLY,
                                            &mca_plm_osvrest_component.pass_environ_mca_params);

    return ORTE_SUCCESS;
}

static int osvrest_component_open(void)
{
    return ORTE_SUCCESS;
}


static int osvrest_component_query(mca_base_module_t **module, int *priority)
{
    /* we are good - make ourselves available */
    *priority = mca_plm_osvrest_component.priority;
    *module = (mca_base_module_t *) &orte_plm_osvrest_module;
    return ORTE_SUCCESS;
}


static int osvrest_component_close(void)
{
    return ORTE_SUCCESS;
}

