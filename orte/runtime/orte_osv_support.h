#ifndef ORTE_OSV_SUPPORT_H
#define ORTE_OSV_SUPPORT_H

#include "orte_config.h"

BEGIN_C_DECLS

ORTE_DECLSPEC int orte_is_osv();


/* From OSv */
// Replacement for fork+exec
ORTE_DECLSPEC int osv_run_app_in_namespace(const char *filename, char *const argv[], char *const envp[]);

END_C_DECLS

#endif
