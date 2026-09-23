#include <config.h>

#include <stddef.h>

#include "bmb_build.h"

const char *bmb_build_version(void)
{
    return PACKAGE_VERSION;
}

const char *bmb_build_backend(void)
{
#ifdef BMB_BLAS_BACKEND
    return BMB_BLAS_BACKEND;
#else
    return "unknown";
#endif
}

#if defined(HAVE_OPENBLAS_GET_CONFIG)
/* Declared here rather than by including a backend header: only OpenBLAS
 * builds resolve this, and cblas.h does not declare it. */
extern char *openblas_get_config(void);

const char *bmb_build_blas_version(void)
{
    return openblas_get_config();
}
#elif defined(HAVE_BLI_INFO_GET_VERSION_STR)
extern const char *bli_info_get_version_str(void);

const char *bmb_build_blas_version(void)
{
    return bli_info_get_version_str();
}
#else
const char *bmb_build_blas_version(void)
{
    return NULL;
}
#endif
