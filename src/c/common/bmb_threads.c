#include <config.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_threads.h"
#include "bmb_build.h"
#include "bmb_log.h"

#if defined(BMB_NO_THREAD_CONTROL)
/* Backend with no runtime thread-count API and no thread-count environment
 * variable of its own (netlib reference BLAS): nothing to set, nothing to
 * reconcile. */
#elif defined(HAVE_FLEXIBLAS_API)
/* FlexiBLAS passes the thread count on to the library it loaded, and has
 * no variable of its own: which ones apply is known only at run time. */
extern void flexiblas_set_num_threads(int num);
#define BMB_THREAD_ENV_AT_RUN_TIME
#elif defined(HAVE_BLI_THREAD_SET_NUM_THREADS)
#include <stdint.h>
/* dim_t defaults to a 64-bit type in BLIS regardless of the CBLAS
 * integer width (BLIS_BLAS_INT_TYPE_SIZE), so declare it explicitly
 * rather than pulling in the full blis.h. */
extern void bli_thread_set_num_threads(int64_t n_threads);
/* BLIS reads BLIS_NUM_THREADS first and falls back to OMP_NUM_THREADS. */
#define BMB_THREAD_ENV_VARS "BLIS_NUM_THREADS", "OMP_NUM_THREADS"
#elif defined(HAVE_OPENBLAS_SET_NUM_THREADS)
extern void openblas_set_num_threads(int num_threads);
/* Same order OpenBLAS itself uses in blas_get_cpu_number(): its own
 * variable, then the GotoBLAS one it inherited, then OMP_NUM_THREADS.
 * Reconciling only the first would let `OMP_NUM_THREADS=4 bmb_dgemm`
 * report a single-threaded run with no warning at all. */
#define BMB_THREAD_ENV_VARS "OPENBLAS_NUM_THREADS", "GOTO_NUM_THREADS", "OMP_NUM_THREADS"
#elif defined(HAVE_OMP_SET_NUM_THREADS)
#include <omp.h>
#define BMB_THREAD_ENV_VARS "OMP_NUM_THREADS"
#endif

#if defined(BMB_THREAD_ENV_AT_RUN_TIME)
/* The variables of the library FlexiBLAS loaded, in that library's order
 * (see below); reference BLAS has none. */
static const char *const *bmb_thread_env_vars(size_t *count)
{
    static const char *const openblas[] = {"OPENBLAS_NUM_THREADS", "GOTO_NUM_THREADS", "OMP_NUM_THREADS"};
    static const char *const blis[] = {"BLIS_NUM_THREADS", "OMP_NUM_THREADS"};
    static const char *const omp[] = {"OMP_NUM_THREADS"};
    const char *backend = bmb_build_backend();

    if (strncmp(backend, "flexiblas/openblas", 18) == 0) {
        *count = sizeof(openblas) / sizeof(openblas[0]);
        return openblas;
    }
    if (strncmp(backend, "flexiblas/blis", 14) == 0) {
        *count = sizeof(blis) / sizeof(blis[0]);
        return blis;
    }
    *count = (strncmp(backend, "flexiblas/netlib", 16) == 0) ? 0 : 1;
    return omp;
}
#elif defined(BMB_THREAD_ENV_VARS)
static const char *const *bmb_thread_env_vars(size_t *count)
{
    static const char *const vars[] = { BMB_THREAD_ENV_VARS };

    *count = sizeof(vars) / sizeof(vars[0]);
    return vars;
}
#endif

void bmb_threads_set(unsigned int count)
{
#if defined(BMB_NO_THREAD_CONTROL)
    if (count > 1) {
        bmb_log_debug("This BLAS backend is single-threaded; --thread-count is ignored.");
    }
#elif defined(HAVE_FLEXIBLAS_API)
    flexiblas_set_num_threads((int) count);
#elif defined(HAVE_BLI_THREAD_SET_NUM_THREADS)
    bli_thread_set_num_threads((int64_t) count);
#elif defined(HAVE_OPENBLAS_SET_NUM_THREADS)
    openblas_set_num_threads((int) count);
#elif defined(HAVE_OMP_SET_NUM_THREADS)
    omp_set_num_threads((int) count);
#else
    if (count > 1) {
        bmb_log_debug("The underlying BLAS backend does not expose a runtime "
                       "thread-count API; --thread-count is ignored.");
    }
#endif
}

void bmb_threads_resolve(bmb_options_t *opts)
{
#if defined(BMB_THREAD_ENV_VARS) || defined(BMB_THREAD_ENV_AT_RUN_TIME)
    size_t n;
    const char *const *env_vars = bmb_thread_env_vars(&n);
    size_t i;

    /* The backend's own precedence order, so the variable reported here is
     * the one it would actually have obeyed. */
    for (i = 0; i < n; i++) {
        const char *env_val = getenv(env_vars[i]);
        unsigned long env_count;
        char *endptr;

        if (env_val == NULL || env_val[0] == '\0') {
            continue;
        }

        /* A plain positive decimal, no larger than the int the library
         * takes. strtoul alone would turn "-2" into 4294967294 threads,
         * and the run would be labelled with it. */
        errno = 0;
        env_count = strtoul(env_val, &endptr, 10);
        if (env_val[0] < '0' || env_val[0] > '9' || *endptr != '\0' || errno != 0
            || env_count == 0 || env_count > (unsigned long) INT_MAX) {
            char msg[256];

            snprintf(msg, sizeof(msg),
                     "%s=\"%.32s\" is not a number of threads; ignored.", env_vars[i], env_val);
            bmb_log_warning(msg);
            continue;
        }

        if (opts->thread_count_set) {
            if (opts->thread_count.count != 1
                || opts->thread_count.values[0] != (size_t) env_count) {
                char requested[128];
                char msg[256];

                bmb_range_describe(&opts->thread_count, requested, sizeof(requested));
                snprintf(msg, sizeof(msg),
                         "%s is ignored! Set to %lu but option -t is set to %s.",
                         env_vars[i], env_count, requested);
                bmb_log_warning(msg);
            }
            return;
        }

        bmb_range_set_single(&opts->thread_count, (size_t) env_count);
        return;
    }
#else
    (void) opts;
#endif
}
