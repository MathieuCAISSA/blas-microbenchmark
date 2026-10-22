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
 * repeated calls (dscal). Run before every timed batch, and always outside
 * the timing window. */
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

/* Optional. The bytes of the operands one call touches: what --cache cold
 * (#160) must keep out of the caches, and so how many copies of them it
 * rotates over. Level 3 routines declare it, having no bytes(); without
 * it, half of bytes() is taken, a lower bound since bytes() counts an
 * operand at most twice, read and written. */
typedef double (*bmb_bench_footprint_fn)(size_t dim1, size_t dim2);

/* --verify: makes one call on the operands the benchmark is about to time
 * and checks its result against a reference (see bmb_verify.h). Returns 0
 * when it is right, 1 when it is wrong -- describing what is wrong in msg
 * -- and -1 when it could not check (out of memory). The driver resets the
 * operands before it, and again before timing, as usual. */
typedef int (*bmb_bench_verify_fn)(void *ctx, char *msg, size_t size);

/* The exit status of a run that --verify stopped on a wrong result. */
#define BMB_EXIT_WRONG_RESULT 2

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

    /* Set when reset() has to run before *every* call rather than once per
     * timed batch, which forces a batch size of 1 and brings the clock's
     * own cost back into the measurement. Only the triangular routines
     * need it: dtrmv and dtrmm multiply their operand by A over and over
     * (it reaches infinity), dtrsv and dtrsm divide by it (it reaches
     * denormals, where the hardware slows down and the timing stops
     * meaning anything). dscal drifts far too slowly to care, so it leaves
     * this at 0 and gets batched. */
    int reset_every_call;

    bmb_bench_flops_fn flops; /* optional, may stay NULL */
    bmb_bench_bytes_fn bytes; /* optional, may stay NULL */
    bmb_bench_footprint_fn footprint; /* optional, may stay NULL */
    bmb_bench_verify_fn verify;
} bmb_benchmark_t;

/* Parses argv, resolves the effective thread count (option vs. env var),
 * sweeps thread-count x size(s), times `iterations` calls (after `warmup`
 * untimed ones) for each combination, then prints/saves the results.
 * Returns a process exit code (EXIT_SUCCESS / EXIT_FAILURE). */
int bmb_benchmark_main(int argc, char *argv[], const bmb_benchmark_t *bench);

/* For setup(): 1 when --layout col asked for column-major matrices, as
 * Fortran stores them, 0 for row-major (the default). A routine with a
 * matrix stores its operands that way, sets its leading dimensions to
 * match, and passes the order to CBLAS and to bmb_verify_matvec(). */
int bmb_bench_column_major(void);

/* For setup(): the bytes of a rows x cols matrix of elements of `size`
 * bytes, or 0 where that would wrap size_t (rows and cols at least 1).
 * The ceiling the parser puts on a size keeps a matrix of 8-byte elements
 * within size_t; a double complex one takes 16, and at -m 1073741824 an
 * M x M one would take 2^64, which wraps to 0: a malloc that succeeds and
 * a setup loop that overruns it. A routine with larger elements checks
 * its matrices with this, and refuses the size with
 * bmb_bench_too_large(). */
size_t bmb_bench_matrix_bytes(size_t rows, size_t cols, size_t size);

/* The first and third quartiles of n sorted samples (n >= 1), which every
 * output gives (#161): each interpolated between the two samples either
 * side of it, as R's default (type 7), NumPy and Excel's QUARTILE.INC do.
 * Exported for its unit test: the samples of a real run cannot be chosen. */
void bmb_quartiles(const double *sorted, unsigned int n, double *q1, double *q3);
void bmb_bench_too_large(void);

/* For setup(): memory for an operand, aligned as --align says (64 bytes
 * by default), or NULL when out of memory; freed with free(). Every
 * operand comes from here (#163), so that two libraries are never
 * compared on operands one of them finds misaligned. */
void *bmb_bench_alloc(size_t bytes);

/* For setup(): the leading dimension of a matrix of `outer` rows
 * (row-major) or columns (column-major) of `inner` elements of `size`
 * bytes each: inner plus --ld-pad, which steps a power-of-two size off
 * the cache-aliasing stride it would otherwise have (#163). The matrix
 * then takes outer * ld * size bytes, from bmb_bench_alloc(). 0 when that
 * would wrap size_t: setup() refuses the size with bmb_bench_too_large().
 * Only elements of more than 8 bytes can: the parser keeps a size plus
 * --ld-pad within the ceiling it sets for 8-byte ones. Its fill and its
 * check go through the leading dimension. */
size_t bmb_bench_ld(size_t outer, size_t inner, size_t size);

#endif /* BMB_BENCH_H */
