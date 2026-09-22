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
 * portion whose wall-clock time is measured, so it must contain nothing
 * but that call -- see reset() below for operand restoration. */
typedef void (*bmb_bench_call_fn)(void *ctx);

/* Optional. Restores the context to the state call() expects, for routines
 * that overwrite an operand (dtrmv, dtrsv, dtrmm, dtrsm) or drift over
 * repeated calls (dscal). Run before every call(), warmup and timed alike,
 * and always outside the timing window. */
typedef void (*bmb_bench_reset_fn)(void *ctx);

/* Releases resources allocated by setup(). */
typedef void (*bmb_bench_teardown_fn)(void *ctx);

/* Optional. Number of floating-point operations in one call, which the
 * harness turns into the GFLOP/s column. These are the conventional BLAS
 * operation counts (2*M*N*K for gemm, K*N*(N+1) for syrk, ...): a count of
 * the arithmetic the routine is defined to perform, not of the
 * instructions a given implementation issues. NULL for routines that do no
 * arithmetic at all (dcopy, dswap). */
typedef double (*bmb_bench_flops_fn)(size_t dim1, size_t dim2);

/* Optional. Bytes one call has to move at minimum -- each operand read
 * once, each output written once -- which the harness turns into the GB/s
 * column. Counted the way STREAM counts it: an output array contributes
 * one write, not the write plus the read-for-ownership the cache actually
 * performs, so that these figures can be compared with STREAM's.
 *
 * Only defined for levels 1 and 2, where operands are streamed once and
 * bandwidth is what limits the routine. Level 3 reuses its operands out of
 * cache (O(N^3) work over O(N^2) data), so a rate computed from compulsory
 * traffic would be a small number that says nothing about achieved
 * bandwidth, and inviting a comparison with STREAM would mislead more than
 * it informs. */
typedef double (*bmb_bench_bytes_fn)(size_t dim1, size_t dim2);

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
    bmb_bench_reset_fn reset; /* optional, may stay NULL */
    bmb_bench_teardown_fn teardown;

    bmb_bench_flops_fn flops; /* optional, may stay NULL */
    bmb_bench_bytes_fn bytes; /* optional, may stay NULL */
} bmb_benchmark_t;

/* Parses argv, resolves the effective thread count (option vs. env var),
 * sweeps thread-count x size(s), times `iterations` calls (after `warmup`
 * untimed ones) for each combination, then prints/saves the results.
 * Returns a process exit code (EXIT_SUCCESS / EXIT_FAILURE). */
int bmb_benchmark_main(int argc, char *argv[], const bmb_benchmark_t *bench);

#endif /* BMB_BENCH_H */
