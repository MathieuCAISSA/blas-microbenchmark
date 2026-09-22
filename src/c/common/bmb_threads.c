#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "bmb_threads.h"
#include "bmb_log.h"

#if defined(BMB_NO_THREAD_CONTROL)
/* Backend with no runtime thread-count API and no thread-count environment
 * variable of its own (netlib reference BLAS): nothing to set, nothing to
 * reconcile. */
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

void bmb_threads_set(unsigned int count)
{
#if defined(BMB_NO_THREAD_CONTROL)
    if (count > 1) {
        bmb_log_debug("This BLAS backend is single-threaded; --thread-count is ignored.");
    }
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
#if defined(BMB_THREAD_ENV_VARS)
    static const char *const env_vars[] = { BMB_THREAD_ENV_VARS };
    size_t i;

    /* The backend's own precedence order, so the variable reported here is
     * the one it would actually have obeyed. */
    for (i = 0; i < sizeof(env_vars) / sizeof(env_vars[0]); i++) {
        const char *env_val = getenv(env_vars[i]);
        unsigned long env_count;
        char *endptr;

        if (env_val == NULL || env_val[0] == '\0') {
            continue;
        }

        env_count = strtoul(env_val, &endptr, 10);
        if (*endptr != '\0' || env_count == 0) {
            continue;
        }

        if (opts->thread_count_set) {
            if (env_count != opts->thread_count.min || env_count != opts->thread_count.max) {
                char msg[256];

                snprintf(msg, sizeof(msg),
                         "%s is ignored! Set to %lu but option -t is set to %zu.",
                         env_vars[i], env_count, opts->thread_count.max);
                bmb_log_warning(msg);
            }
            return;
        }

        opts->thread_count.min = (size_t) env_count;
        opts->thread_count.max = (size_t) env_count;
        return;
    }
#else
    (void) opts;
#endif
}
