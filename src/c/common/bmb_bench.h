#ifndef BMB_BENCH_H
#define BMB_BENCH_H

#include <stddef.h>

#include "bmb_options.h"

/* Allocates and fills the routine's inputs for the given size(s) and
 * thread count. Returns an opaque context passed to call()/teardown(),
 * or NULL on allocation failure. dim2 is 0 when the routine has no
 * second dimension (dim2_label == NULL below). */
typedef void *(*bmb_bench_setup_fn)(size_t dim1, size_t dim2, unsigned int thread_count);

/* Executes exactly one call to the BLAS routine under test. This is the
 * portion whose wall-clock time is measured. */
typedef void (*bmb_bench_call_fn)(void *ctx);

/* Releases resources allocated by setup(). */
typedef void (*bmb_bench_teardown_fn)(void *ctx);

typedef struct {
    const char *routine_name;

    /* Column label for the routine's primary size dimension. */
    const char *dim1_label;
    /* Column label for a second, independent size dimension (e.g. GEMM's
     * N, or SYRK's K), or NULL if the routine has a single dimension
     * (e.g. a vector length, or a square matrix's N). */
    const char *dim2_label;

    /* 1: dim1 is swept over --vector-size (level 1 routines).
     * 0: dim1 is swept over --matrix-dim1 (level 2 & 3 routines). */
    int use_vector_range;

    bmb_bench_setup_fn setup;
    bmb_bench_call_fn call;
    bmb_bench_teardown_fn teardown;
} bmb_benchmark_t;

/* Parses argv, resolves the effective thread count (option vs. env var),
 * sweeps thread-count x size(s), times `iterations` calls (after `warmup`
 * untimed ones) for each combination, then prints/saves the results.
 * Returns a process exit code (EXIT_SUCCESS / EXIT_FAILURE). */
int bmb_benchmark_main(int argc, char *argv[], const bmb_benchmark_t *bench);

#endif /* BMB_BENCH_H */
