#include <config.h>

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

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

/* icx is a clang that names itself in __VERSION__ only; clang's own
 * __VERSION__ started with "4.2.1 Compatible" up to clang 13, and gcc's
 * holds the version alone. */
const char *bmb_build_compiler(void)
{
#if defined(__INTEL_LLVM_COMPILER)
    return __VERSION__;
#elif defined(__clang__)
    return "clang " __clang_version__;
#elif defined(__GNUC__)
    return "gcc " __VERSION__;
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
}

const char *bmb_build_cflags(void)
{
    return BMB_CFLAGS;
}

void bmb_build_backend_name(const char *library, char *buf, size_t size)
{
    static const char prefix[] = "flexiblas/";
    size_t i, n = 0;

    if (size < sizeof(prefix)) {
        if (size > 0) {
            buf[0] = '\0';
        }
        return;
    }
    memcpy(buf, prefix, sizeof(prefix) - 1);
    n = sizeof(prefix) - 1;
    for (i = 0; library[i] != '\0' && n + 1 < size; i++) {
        const char c = (char) tolower((unsigned char) library[i]);

        buf[n++] = (isalnum((unsigned char) c) || strchr("._+-", c) != NULL) ? c : '-';
    }
    if (i == 0 && n + 7 < size) {
        memcpy(buf + n, "unknown", 7);
        n += 7;
    }
    buf[n] = '\0';
}

const char *bmb_build_backend(void)
{
#if defined(HAVE_FLEXIBLAS_API)
    static char name[96];

    if (name[0] == '\0') {
        bmb_build_backend_name(bmb_flexiblas_current(), name, sizeof(name));
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
#elif defined(HAVE_MKL_GET_VERSION_STRING)
#include <mkl_service.h>

const char *bmb_build_blas_version(void)
{
    static char version[256];

    if (version[0] == '\0') {
        mkl_get_version_string(version, (int) sizeof(version));
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

#if defined(HAVE_OPENBLAS_GET_CORENAME)
extern char *openblas_get_corename(void);

const char *bmb_build_kernels(void)
{
    return openblas_get_corename();
}
#elif defined(HAVE_BLI_ARCH_QUERY_ID)
/* arch_t is an enum, passed as an int. */
extern int bli_arch_query_id(void);
extern const char *bli_arch_string(int id);

const char *bmb_build_kernels(void)
{
    return bli_arch_string(bli_arch_query_id());
}
#elif defined(HAVE_MKL_CBWR_GET_AUTO_BRANCH)
#include <mkl_service.h>

/* The branch MKL's dispatch takes on this CPU, under
 * MKL_ENABLE_INSTRUCTIONS if it is set. The constants a release does not
 * have are left out. */
const char *bmb_build_kernels(void)
{
    switch (mkl_cbwr_get_auto_branch()) {
    case MKL_CBWR_COMPATIBLE:
        return "COMPATIBLE";
    case MKL_CBWR_SSE2:
        return "SSE2";
#if defined(MKL_CBWR_SSE4_2)
    case MKL_CBWR_SSE4_2:
        return "SSE4_2";
#endif
#if defined(MKL_CBWR_AVX2)
    case MKL_CBWR_AVX2:
        return "AVX2";
#endif
#if defined(MKL_CBWR_AVX512)
    case MKL_CBWR_AVX512:
        return "AVX512";
#endif
#if defined(MKL_CBWR_AVX512_E1)
    case MKL_CBWR_AVX512_E1:
        return "AVX512_E1";
#endif
#if defined(MKL_CBWR_AVX10)
    case MKL_CBWR_AVX10:
        return "AVX10";
#endif
    default:
        return "unknown branch";
    }
}
#endif
