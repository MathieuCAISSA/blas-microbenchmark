#include <config.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "bmb_build.h"

#if defined(HAVE_FLEXIBLAS_API)
/* FlexiBLAS loads the BLAS library at run time, so the backend and its
 * version are read from it, once, not fixed by configure: one build then
 * gives a "flexiblas/openblas-openmp" and a "flexiblas/blis-openmp"
 * series, which the report keeps apart. Declared here rather than by
 * including flexiblas_api.h, whose location varies. */
extern int flexiblas_current_backend(char *name, size_t len);
extern void flexiblas_get_version(int *major, int *minor, int *patch);

static char bmb_flexiblas_loaded[64];

/* The library FlexiBLAS loaded, as it names it ("OPENBLAS-OPENMP"). */
static const char *bmb_flexiblas_current(void)
{
    if (bmb_flexiblas_loaded[0] == '\0'
        && flexiblas_current_backend(bmb_flexiblas_loaded, sizeof(bmb_flexiblas_loaded)) < 0) {
        bmb_flexiblas_loaded[0] = '\0';
    }
    return bmb_flexiblas_loaded[0] != '\0' ? bmb_flexiblas_loaded : "unknown";
}
#endif

const char *bmb_build_version(void)
{
    return PACKAGE_VERSION;
}

const char *bmb_build_backend(void)
{
#if defined(HAVE_FLEXIBLAS_API)
    static char name[96];

    if (name[0] == '\0') {
        const char *loaded = bmb_flexiblas_current();
        size_t i, n = (size_t) snprintf(name, sizeof(name), "flexiblas/");

        for (i = 0; loaded[i] != '\0' && n + 1 < sizeof(name); i++) {
            name[n++] = (char) tolower((unsigned char) loaded[i]);
        }
        name[n] = '\0';
    }
    return name;
#elif defined(BMB_BLAS_BACKEND)
    return BMB_BLAS_BACKEND;
#else
    return "unknown";
#endif
}

#if defined(HAVE_FLEXIBLAS_API)
const char *bmb_build_blas_version(void)
{
    static char version[128];

    if (version[0] == '\0') {
        int major = 0, minor = 0, patch = 0;

        flexiblas_get_version(&major, &minor, &patch);
        snprintf(version, sizeof(version), "FlexiBLAS %d.%d.%d, %s", major, minor, patch,
                 bmb_flexiblas_current());
    }
    return version;
}
#elif defined(HAVE_OPENBLAS_GET_CONFIG)
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
