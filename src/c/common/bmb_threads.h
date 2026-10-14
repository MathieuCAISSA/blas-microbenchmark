#ifndef BMB_THREADS_H
#define BMB_THREADS_H

#include "bmb_options.h"

/* Reconciles opts->thread_count with the BLAS backend's thread-count
 * environment variable (OMP_NUM_THREADS, OPENBLAS_NUM_THREADS, ...).
 *
 * If -t/--thread-count was not given on the command line, the env var
 * (when set) becomes the effective thread count. If -t was given and the
 * env var is also set to a different value, a warning is logged and the
 * command-line value wins. Must be called once, after bmb_options_parse(). */
void bmb_threads_resolve(bmb_options_t *opts);

/* Warns when the run asks the backend for more threads than the
 * `usable_cpus` this process may run on (bmb_machine_t; 0 is unknown):
 * they would take turns on the CPUs, and the timings would measure that.
 * Silent for a backend that takes no thread count. */
void bmb_threads_check_usable(const bmb_options_t *opts, long usable_cpus);

/* Applies the given thread count to the underlying BLAS backend, when the
 * backend exposes a runtime thread-count API. No-op otherwise. */
void bmb_threads_set(unsigned int count);

#endif /* BMB_THREADS_H */
