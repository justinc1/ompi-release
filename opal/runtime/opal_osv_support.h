#ifndef OPAL_OSV_SUPPORT_H
#define OPAL_OSV_SUPPORT_H

#include "opal_config.h"

BEGIN_C_DECLS

OPAL_DECLSPEC int opal_is_osv();


/* From OSv */
// Replacement for fork+exec
OPAL_DECLSPEC int osv_run_app_in_namespace(const char *filename, char *const argv[], char *const envp[]);

END_C_DECLS

#endif
