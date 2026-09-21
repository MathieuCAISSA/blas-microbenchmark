#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include "bmb_threads.h"
#include "bmb_log.h"

#if defined(HAVE_BLI_THREAD_SET_NUM_THREADS)
#include <stdint.h>
/* dim_t defaults to a 64-bit type in BLIS regardless of the CBLAS
 * integer width (BLIS_BLAS_INT_TYPE_SIZE), so declare it explicitly
 * rather than pulling in the full blis.h. */
extern void bli_thread_set_num_threads(int64_t n_threads);
#define BMB_THREAD_ENV_VAR "BLIS_NUM_THREADS"
#elif defined(HAVE_OPENBLAS_SET_NUM_THREADS)
extern void openblas_set_num_threads(int num_threads);
#define BMB_THREAD_ENV_VAR "OPENBLAS_NUM_THREADS"
#elif defined(HAVE_OMP_SET_NUM_THREADS)
#include <omp.h>
#define BMB_THREAD_ENV_VAR "OMP_NUM_THREADS"
#endif

void bmb_threads_set(unsigned int count)
{
#if defined(HAVE_BLI_THREAD_SET_NUM_THREADS)
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
#if defined(BMB_THREAD_ENV_VAR)
    const char *env_val = getenv(BMB_THREAD_ENV_VAR);
    unsigned long env_count;
    char *endptr;

    if (env_val == NULL || env_val[0] == '\0') {
        return;
    }

    env_count = strtoul(env_val, &endptr, 10);
    if (*endptr != '\0' || env_count == 0) {
        return;
    }

    if (opts->thread_count_set) {
        if (env_count != opts->thread_count.min || env_count != opts->thread_count.max) {
            char msg[256];

            snprintf(msg, sizeof(msg),
                     "%s is ignored! Set to %lu but option -t is set to %zu.",
                     BMB_THREAD_ENV_VAR, env_count, opts->thread_count.max);
            bmb_log_warning(msg);
        }
        return;
    }

    opts->thread_count.min = (size_t) env_count;
    opts->thread_count.max = (size_t) env_count;
#else
    (void) opts;
#endif
}
