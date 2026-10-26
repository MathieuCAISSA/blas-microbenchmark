#define _POSIX_C_SOURCE 200809L

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_bench.h"
#include "bmb_build.h"
#include "bmb_log.h"
#include "bmb_machine.h"
#include "bmb_print.h"
#include "bmb_result.h"
#include "bmb_threads.h"
#include "bmb_timer.h"

/* The reported time is the fastest sample, not the mean.
 *
 * A sample is already an average over a whole batch of calls, so the
 * fastest one is not a lucky single call -- it is the batch that ran with
 * the least interference. That matters: a batch lasts long enough that one
 * scheduling hiccup on a busy machine inflates a sample by a factor of
 * fifty, and with a handful of samples that drags the mean with it. Runs
 * of ddot at n=1024 on the development machine gave means of 2949, 396 and
 * 131 ns while their fastest samples were 163, 121 and 127 ns.
 *
 * The median and the quartiles go with it into every output, so that how
 * steady the samples were is never lost (#161); the mean, the standard
 * deviation and the slowest sample are added under -s. */
static int bmb_compare_times(const void *a, const void *b)
{
    const double x = *(const double *) a, y = *(const double *) b;

    return (x > y) - (x < y);
}

static double bmb_quantile(const double *sorted, unsigned int n, double p)
{
    const double h = (double) (n - 1) * p;
    const unsigned int i = (unsigned int) h;

    if (i + 1 >= n) {
        return sorted[n - 1];
    }
    return sorted[i] + (h - (double) i) * (sorted[i + 1] - sorted[i]);
}

/* The median of n sorted samples, computed apart from the quartiles so
 * that two give their mean to the last bit. */
static double bmb_median(const double *sorted, unsigned int n)
{
    return (n % 2 == 1) ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
}

void bmb_quartiles(const double *sorted, unsigned int n, double *q1, double *q3)
{
    *q1 = bmb_quantile(sorted, n, 0.25);
    *q3 = bmb_quantile(sorted, n, 0.75);
}

/* Sorts `times`, for the median: the samples are not used afterwards. */
static void bmb_compute_stats(double *times, unsigned int n, bmb_result_row_t *row)
{
    unsigned int i;
    double sum = 0.0;
    double variance = 0.0;

    row->time_s = times[0];
    row->max_s = times[0];
    for (i = 0; i < n; i++) {
        sum += times[i];
        if (times[i] < row->time_s) {
            row->time_s = times[i];
        }
        if (times[i] > row->max_s) {
            row->max_s = times[i];
        }
    }
    row->mean_s = sum / (double) n;

    for (i = 0; i < n; i++) {
        double d = times[i] - row->mean_s;
        variance += d * d;
    }
    /* Population variance (divided by n, not n-1): the n samples are the
     * whole of what was measured, not a sample drawn from it. */
    variance /= (double) n;
    row->stddev_s = sqrt(variance);

    qsort(times, n, sizeof(*times), bmb_compare_times);
    row->median_s = bmb_median(times, n);
    bmb_quartiles(times, n, &row->q1_s, &row->q3_s);
}

/* How many calls go into one timed batch.
 *
 * Wrapping every single call in a pair of clock reads makes the clock part
 * of what gets reported: an empty timed region costs tens of nanoseconds
 * on a typical x86 box, which is the entire duration of a small level 1
 * call. Timing a batch of B calls and dividing by B spreads that cost over
 * the whole batch.
 *
 * B is chosen so a batch lasts about BMB_BATCH_TARGET_S. Anything already
 * longer than that gets B = 1 and is timed exactly as before, so nothing
 * changes for the sizes where the clock was never a problem. */
#define BMB_BATCH_TARGET_S 1.0e-4
#define BMB_BATCH_MAX      1000000u

static unsigned int bmb_batch_from(double per_call)
{
    double batch;

    /* Too short for the clock to resolve at all: take the largest batch
     * and let the second pass work from a real measurement. */
    if (per_call <= 0.0) { /* GCOVR_EXCL_BR_LINE: a call too short for the clock to resolve: not on Linux's nanosecond clock */
        return BMB_BATCH_MAX; /* GCOVR_EXCL_LINE: a call too short for the clock to resolve: not on Linux's nanosecond clock */
    }

    batch = BMB_BATCH_TARGET_S / per_call;
    if (batch <= 1.0) {
        return 1;
    }
    if (batch >= (double) BMB_BATCH_MAX) { /* GCOVR_EXCL_BR_LINE: 10^6 calls in 0.1 ms, a call of 0.1 ns: none is that short */
        return BMB_BATCH_MAX; /* GCOVR_EXCL_LINE: 10^6 calls in 0.1 ms, a call of 0.1 ns: none is that short */
    }
    return (unsigned int) batch;
}

/* The operands a point is timed on: one set, reused from call to call, as
 * a code calling the routine in a loop would; or, under --cache cold
 * (#160), `count` copies of them, each call taking the next, so that the
 * others, used in between, have pushed it out of the caches. */
typedef struct {
    void **ctx;
    unsigned int count;
    unsigned int next;   /* the copy the next call takes */
    int cold;
} bmb_operands_t;

/* How far past the last-level cache the copies other than the one in use
 * reach: twice it, so that what a cache keeps of data it has seen go by
 * is gone too. And how many copies at most, for the smallest sizes, where
 * the operands are a few bytes: past that, a point is partly warm, and a
 * warning says so. */
#define BMB_COLD_REACH    2.0
#define BMB_COLD_MAX_COPIES 16384u

/* The copies --cache cold rotates over at a point: 1 when the operands
 * alone are past the reach, as each call then pushes its own out. */
static unsigned int bmb_cold_copies(const bmb_benchmark_t *bench, size_t dim1, size_t dim2,
                                    unsigned long long llc)
{
    const double footprint = (bench->footprint != NULL) ? bench->footprint(dim1, dim2)
                                                        : bench->bytes(dim1, dim2) / 2.0;
    const double reach = BMB_COLD_REACH * (double) llc;
    double copies;

    if (footprint >= reach) {
        return 1;
    }
    copies = ceil(reach / footprint) + 1.0;
    return (copies > (double) BMB_COLD_MAX_COPIES) ? BMB_COLD_MAX_COPIES : (unsigned int) copies;
}

/* Restores, after a batch, the copies it used, in the order they will be
 * used next: under --cache cold, a reset() before the call would bring
 * the operand it restores back into the cache, so it runs right after the
 * call instead, and the copies used before the next call on this one push
 * it back out. */
static void bmb_reset_used(const bmb_benchmark_t *bench, const bmb_operands_t *ops,
                           unsigned int batch)
{
    const unsigned int used = (batch < ops->count) ? batch : ops->count;
    const unsigned int first = (ops->next + ops->count - used) % ops->count;
    unsigned int j;

    for (j = 0; j < used; j++) {
        bench->reset(ops->ctx[(first + j) % ops->count]);
    }
}

static double bmb_time_batch(const bmb_benchmark_t *bench, bmb_operands_t *ops, unsigned int batch)
{
    double t0, t1;
    unsigned int b;

    if (bench->reset != NULL && !ops->cold) {
        bench->reset(ops->ctx[0]);
    }

    /* One set of operands keeps the loop it always had: a call of a few
     * nanoseconds would carry the rotation's bookkeeping. */
    t0 = bmb_timer_now();
    if (ops->count == 1) {
        for (b = 0; b < batch; b++) {
            bench->call(ops->ctx[0]);
        }
    } else {
        for (b = 0; b < batch; b++) {
            bench->call(ops->ctx[ops->next]);
            if (++ops->next == ops->count) {
                ops->next = 0;
            }
        }
    }
    t1 = bmb_timer_now();

    if (bench->reset != NULL && ops->cold) {
        bmb_reset_used(bench, ops, batch);
    }

    return (t1 - t0) / (double) batch;
}

static unsigned int bmb_choose_batch(const bmb_benchmark_t *bench, bmb_operands_t *ops)
{
    unsigned int batch;

    if (bench->reset_every_call) {
        return 1;
    }

    /* Two passes: the first call is timed individually, so its own reading
     * carries the clock's cost and comes out too high -- which would leave
     * the batch too small and some of that cost still in the result. The
     * second pass re-measures with a real batch, where the cost is already
     * negligible, and lands on the right size. */
    batch = bmb_batch_from(bmb_time_batch(bench, ops, 1));
    if (batch == 1) {
        return 1;
    }

    return bmb_batch_from(bmb_time_batch(bench, ops, batch));
}


/* The point's K under -K (#215), set around each point it applies to; 0
 * when dim1 stands for it. */
static size_t bmb_dim3;

/* --verify, for one point: one call on the operands about to be timed,
 * checked against a reference. 0 when the result is right, -2 when it is
 * wrong, -1 when it could not be checked. */
static int bmb_verify_point(const bmb_benchmark_t *bench, void *ctx,
                            unsigned int thread_count, size_t dim1, size_t dim2)
{
    char what[512];
    char where[256];
    char msg[1024];
    int v;

    if (bmb_dim3 != 0) {
        snprintf(where, sizeof(where), "%s, %s %zu, %s %zu, %s %zu, %u thread%s",
                 bench->routine_name, bench->dim1_label_dim3, dim1, bench->dim2_label, dim2,
                 bench->dim3_label, bmb_dim3, thread_count, thread_count == 1 ? "" : "s");
    } else if (bench->dim2_label != NULL) {
        snprintf(where, sizeof(where), "%s, %s %zu, %s %zu, %u thread%s", bench->routine_name,
                 bench->dim1_label, dim1, bench->dim2_label, dim2,
                 thread_count, thread_count == 1 ? "" : "s");
    } else {
        snprintf(where, sizeof(where), "%s, %s %zu, %u thread%s", bench->routine_name,
                 bench->dim1_label, dim1, thread_count, thread_count == 1 ? "" : "s");
    }

    if (bench->reset != NULL) {
        bench->reset(ctx);
    }
    what[0] = '\0';
    v = bench->verify(ctx, what, sizeof(what));
    if (v < 0) {
        snprintf(msg, sizeof(msg), "%s: the result could not be checked (out of memory).", where);
        bmb_log_error(msg);
        return -1;
    }
    if (v > 0) {
        snprintf(msg, sizeof(msg), "Wrong result: %s: %s. Nothing measured from here on.", where, what);
        bmb_log_error(msg);
        return -2;
    }
    return 0;
}

static void bmb_operands_free(const bmb_benchmark_t *bench, bmb_operands_t *ops)
{
    unsigned int i;

    for (i = 0; i < ops->count; i++) {
        bench->teardown(ops->ctx[i]);
    }
    free(ops->ctx);
}

/* Sets up the operands of a point: one set, or under --cache cold as many
 * copies as bmb_cold_copies() says. 0, or -1 when one could not be made,
 * those already made freed. */
static int bmb_operands_setup(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                              unsigned int thread_count, size_t dim1, size_t dim2,
                              unsigned long long llc, bmb_operands_t *ops)
{
    const unsigned int count = opts->cache_cold ? bmb_cold_copies(bench, dim1, dim2, llc) : 1;

    ops->cold = opts->cache_cold;
    ops->next = 0;
    ops->count = 0;
    ops->ctx = malloc(count * sizeof(*ops->ctx));
    if (ops->ctx == NULL) {
        bmb_log_error("Out of memory while allocating the copies of the operands.");
        return -1;
    }
    while (ops->count < count) {
        void *ctx = bench->setup(dim1, dim2, thread_count);

        if (ctx == NULL) {
            bmb_log_error("Benchmark setup failed (out of memory?); skipping this data point.");
            bmb_operands_free(bench, ops);
            return -1;
        }
        ops->ctx[ops->count++] = ctx;
    }
    return 0;
}

/* What --cache cold could not do at the smallest sizes, said once a run:
 * the copies it is allowed do not reach past the last-level cache. */
static void bmb_warn_partly_warm(const bmb_benchmark_t *bench, size_t dim1, size_t dim2,
                                 unsigned long long llc)
{
    static int warned;
    const double footprint = (bench->footprint != NULL) ? bench->footprint(dim1, dim2)
                                                        : bench->bytes(dim1, dim2) / 2.0;
    char msg[512];

    if (warned || footprint * (double) (BMB_COLD_MAX_COPIES - 1) >= BMB_COLD_REACH * (double) llc) {
        return;
    }
    warned = 1;
    snprintf(msg, sizeof(msg),
             "--cache cold: from %s %zu down, %u copies of the operands do not reach twice the "
             "last-level cache; those points are partly warm (\"copies\" in the results).",
             bench->dim1_label, dim1, BMB_COLD_MAX_COPIES);
    bmb_log_warning(msg);
}

/* The last-level cache --cache cold must reach past, read once a run. */
static unsigned long long bmb_llc;

/* Whether the next point is the first of its thread count, which
 * --warmup-time warms up before (#164). */
static int bmb_warm_pending;

/* The most samples --min-time and --max-spread take at a point: the
 * buffer for them is set aside whole, 800 KB, rather than grown as they
 * come. And the fewest --max-spread judges a spread from. */
#define BMB_MAX_SAMPLES   100000u
#define BMB_SPREAD_SAMPLES 10u

/* Whether the n samples taken, which add up to `total` seconds, are
 * enough (#164): --min-time reached, then --max-spread, unless --max-time
 * is. The spread is the interquartile range over the median, not the
 * coefficient of variation: one preempted sample in thousands keeps a
 * standard deviation, and so the CV, from ever settling. It is judged
 * each time the samples have doubled since it was last, so that sorting
 * them costs O(n log n) in all; *check is when it is next. Sorts
 * `times`, whose order nothing after depends on. */
static int bmb_samples_enough(const bmb_options_t *opts, double *times, unsigned int n,
                              double total, unsigned int *check)
{
    double q1, q3;

    if (total >= opts->max_time) {
        return 1;
    }
    if (total < opts->min_time) {
        return 0;
    }
    if (opts->max_spread <= 0.0) {
        return 1;
    }
    if (n < *check) {
        return 0;
    }
    *check = 2 * n;
    qsort(times, n, sizeof(*times), bmb_compare_times);
    bmb_quartiles(times, n, &q1, &q3);
    return q3 - q1 <= opts->max_spread / 100.0 * bmb_median(times, n);
}

static int bmb_run_one(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                        unsigned int thread_count, size_t dim1, size_t dim2,
                        bmb_result_row_t *row)
{
    bmb_operands_t ops;
    double *times;
    double total = 0.0;
    const int adaptive = opts->min_time > 0.0 || opts->max_spread > 0.0;
    const unsigned int capacity = (adaptive && opts->iterations < BMB_MAX_SAMPLES) ? BMB_MAX_SAMPLES
                                                                                   : opts->iterations;
    unsigned int batch;
    unsigned int i, n;
    unsigned int check = BMB_SPREAD_SAMPLES;
    int v;

    bmb_threads_set(thread_count);

    if (bmb_operands_setup(bench, opts, thread_count, dim1, dim2, bmb_llc, &ops) != 0) {
        return -1;
    }
    if (ops.cold) {
        bmb_warn_partly_warm(bench, dim1, dim2, bmb_llc);
    }

    /* Checked on the very operands that are timed next, at this size and
     * thread count: a library can be right at one and wrong at another. */
    if (opts->verify) {
        v = bmb_verify_point(bench, ops.ctx[0], thread_count, dim1, dim2);
        if (v != 0) {
            bmb_operands_free(bench, &ops);
            return v;
        }
        /* Under --cache cold, the copy --verify called is restored and
         * goes last, so the next call does not find it just restored. */
        if (ops.cold) {
            if (bench->reset != NULL) {
                bench->reset(ops.ctx[0]);
            }
            ops.next = 1 % ops.count;
        }
    }

    /* Before the first point of a thread count, calls for --warmup-time:
     * the CPU leaves its idle clock, the library its threads' sleep. */
    if (bmb_warm_pending) {
        const double t0 = bmb_timer_now();

        while (bmb_timer_now() - t0 < opts->warmup_time) {
            (void) bmb_time_batch(bench, &ops, 1);
        }
        bmb_warm_pending = 0;
    }
    for (i = 0; i < opts->warmup; i++) {
        (void) bmb_time_batch(bench, &ops, 1);
    }

    times = malloc(capacity * sizeof(*times));
    if (times == NULL) {
        bmb_log_error("Out of memory while allocating timing buffer.");
        bmb_operands_free(bench, &ops);
        return -1;
    }

    /* A routine whose operand must be restored before every call is timed
     * one call at a time, whatever -b says: N calls in a row would run on
     * an operand drifting towards infinity or denormals (#48). */
    batch = (opts->batch != 0 && !bench->reset_every_call) ? opts->batch
                                                           : bmb_choose_batch(bench, &ops);

    /* Each sample is the mean over `batch` calls. reset() runs before the
     * batch, or after it under --cache cold, never inside it: restoring an
     * operand is bookkeeping, and for dtrmm/dtrsm it is an O(M*N) copy that
     * would otherwise be a large part of what gets reported as the
     * routine's time. */
    for (n = 0; n < capacity;) {
        times[n] = bmb_time_batch(bench, &ops, batch);
        total += times[n++] * (double) batch;
        if (n >= opts->iterations && bmb_samples_enough(opts, times, n, total, &check)) {
            break;
        }
    }

    row->copies = ops.count;
    row->samples = n;
    bmb_operands_free(bench, &ops);

    row->thread_count = thread_count;
    row->dim1 = dim1;
    row->dim2 = dim2;
    row->dim3 = bmb_dim3;
    row->batch = batch;
    bmb_compute_stats(times, n, row);

    /* Rates come from the reported time, i.e. the fastest sample. GFLOP/s
     * and GB/s are decimal (1e9), as they conventionally are for compute
     * and bandwidth figures. */
    row->gflops = 0.0;
    row->gbytes_s = 0.0;
    if (row->time_s > 0.0) { /* GCOVR_EXCL_BR_LINE: a call too short for the clock to resolve: not on Linux's nanosecond clock */
        if (bench->flops != NULL) {
            row->gflops = bench->flops(dim1, dim2) / row->time_s / 1.0e9;
        }
        if (bench->bytes != NULL) {
            row->gbytes_s = bench->bytes(dim1, dim2) / row->time_s / 1.0e9;
        }
    }

    free(times);
    return 0;
}

/* BMB_COUNTS, a testing aid as BMB_VERIFY_CORRUPT is: each point's
 * counts, as the routine declares them, written to stderr in place of
 * measuring it, so that nothing is allocated whatever the size.
 * test_bmb_counts.sh holds them to the BLAS conventions (#205). */
static int bmb_counts_only;

static void bmb_print_count(const char *name, double (*count)(size_t, size_t),
                            size_t dim1, size_t dim2)
{
    if (count != NULL) {
        fprintf(stderr, " %s %.17g", name, count(dim1, dim2));
    } else {
        fprintf(stderr, " %s -", name);
    }
}

static int bmb_record(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                       unsigned int thread_count, size_t dim1, size_t dim2,
                       bmb_result_set_t *rs)
{
    bmb_result_row_t row;
    int r;

    if (bmb_counts_only) {
        fprintf(stderr, "counts: %s %zu %zu", bench->routine_name, dim1, dim2);
        if (bmb_dim3 != 0) {
            fprintf(stderr, " K %zu", bmb_dim3);
        }
        bmb_print_count("flops", bench->flops, dim1, dim2);
        bmb_print_count("bytes", bench->bytes, dim1, dim2);
        bmb_print_count("footprint", bench->footprint, dim1, dim2);
        fputc('\n', stderr);
        return 0;
    }
    r = bmb_run_one(bench, opts, thread_count, dim1, dim2, &row);
    if (r != 0) {
        return r;
    }

    bmb_print_txt_row(stdout, rs, &row);

    /* Kept as well, because the file output needs the whole set: a failure
     * here costs the file, not the row the user has already seen. */
    if (bmb_result_set_add(rs, &row) != 0) {
        bmb_log_error("Out of memory while recording results.");
        return -1;
    }

    return 0;
}

/* One (dim1, dim2) point, at every K -K gives for a routine that has
 * one apart (#215), or once. */
static int bmb_record_k(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                        unsigned int thread_count, size_t dim1, size_t dim2,
                        bmb_result_set_t *rs)
{
    size_t i3;
    int r = 0;

    if (bench->dim3_label == NULL || !opts->matrix_dim3_set) {
        return bmb_record(bench, opts, thread_count, dim1, dim2, rs);
    }
    for (i3 = 0; i3 < opts->matrix_dim3.count && r != -2; i3++) {
        int r3;

        bmb_dim3 = opts->matrix_dim3.values[i3];
        r3 = bmb_record(bench, opts, thread_count, dim1, dim2, rs);
        bmb_dim3 = 0;
        if (r3 != 0) {
            r = r3;
        }
    }
    return r;
}

static int bmb_sweep_dim1(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                           unsigned int thread_count, bmb_result_set_t *rs)
{
    const bmb_range_t *range1 = bench->use_vector_range ? &opts->vector_size : &opts->matrix_dim1;
    const bmb_range_t *range2 = &opts->matrix_dim2;
    size_t i1;
    int status = 0;

    for (i1 = 0; i1 < range1->count; i1++) {
        size_t d1 = range1->values[i1];

        int r = 0;

        if (bench->dim2_label != NULL && !opts->matrix_dim2_set) {
            /* No -M: square matrices, dim2 following dim1 point by point.
             * Taking -M's *range* from -m instead, as this once did, made
             * `-m 512:2048` measure all nine M x N combinations where three
             * were asked for -- N^2 runs for N sizes, most of them shapes
             * nobody requested, while --help promised square matrices. */
            r = bmb_record_k(bench, opts, thread_count, d1, d1, rs);
        } else if (bench->dim2_label != NULL) {
            size_t i2;

            /* -M given: every combination, the grid a heatmap needs. */
            for (i2 = 0; i2 < range2->count && r != -2; i2++) {
                int r2 = bmb_record_k(bench, opts, thread_count, d1, range2->values[i2], rs);

                if (r2 != 0) {
                    r = r2;
                }
            }
        } else {
            r = bmb_record(bench, opts, thread_count, d1, 0, rs);
        }
        /* A wrong result ends the run: numbers for a library that computes
         * the wrong thing are not worth having. */
        if (r == -2) {
            return -2;
        }
        if (r != 0) {
            status = -1;
        }
    }

    return status;
}

/* A size option that this particular routine has no use for would
 * otherwise be accepted and quietly dropped, leaving the user reading
 * numbers for a size they never asked for. */
static int bmb_column_major;

int bmb_bench_column_major(void)
{
    return bmb_column_major;
}

size_t bmb_bench_dim3(size_t dim1)
{
    return bmb_dim3 != 0 ? bmb_dim3 : dim1;
}

/* The run's arguments (#221), the defaults filled in by
 * bmb_check_args(). */
static char bmb_trans[3] = "NN";
static char bmb_uplo = 'U';
static char bmb_side = 'L';
static char bmb_diag = 'N';

char bmb_bench_trans(int i)
{
    return bmb_trans[i];
}

char bmb_bench_uplo(void)
{
    return bmb_uplo;
}

char bmb_bench_side(void)
{
    return bmb_side;
}

char bmb_bench_diag(void)
{
    return bmb_diag;
}

static unsigned int bmb_batch_count = 1;
static int bmb_batch_mode = BMB_BATCH_LOOP;

unsigned int bmb_bench_batch_count(void)
{
    return bmb_batch_count;
}

int bmb_bench_batch_mode(void)
{
    return bmb_batch_mode;
}

/* An argument the routine does not have is refused, never ignored: a
 * --uplo L that changed nothing would be read as the lower triangle's
 * number. */
static int bmb_check_args(const bmb_benchmark_t *bench, const bmb_options_t *opts)
{
    const size_t want = (bench->args & BMB_ARG_TRANS2) ? 2 : 1;
    char msg[256];
    size_t i;

    if (opts->trans[0] != '\0') {
        if (!(bench->args & (BMB_ARG_TRANS | BMB_ARG_TRANS2))) {
            snprintf(msg, sizeof(msg), "--trans: %s has no matrix to transpose.", bench->routine_name);
            bmb_log_error(msg);
            return -1;
        }
        if (strlen(opts->trans) != want) {
            snprintf(msg, sizeof(msg), "--trans: %s takes %s.", bench->routine_name,
                     want == 2 ? "two letters, A's then B's (NT, TN, ...)" : "one letter");
            bmb_log_error(msg);
            return -1;
        }
        for (i = 0; i < want; i++) {
            if (strchr(bench->trans_letters, opts->trans[i]) == NULL) {
                snprintf(msg, sizeof(msg), "--trans: %s takes %s, not %c%s.", bench->routine_name,
                         bench->trans_letters, opts->trans[i],
                         opts->trans[i] == 'C' ? " (a real matrix's conjugate transpose is T)" : "");
                bmb_log_error(msg);
                return -1;
            }
        }
        memcpy(bmb_trans, opts->trans, want);
    }
    if (opts->uplo != 0 && !(bench->args & BMB_ARG_UPLO)) {
        snprintf(msg, sizeof(msg), "--uplo: %s has no triangle to choose.", bench->routine_name);
        bmb_log_error(msg);
        return -1;
    }
    if (opts->side != 0 && !(bench->args & BMB_ARG_SIDE)) {
        snprintf(msg, sizeof(msg), "--side: %s has no side to choose.", bench->routine_name);
        bmb_log_error(msg);
        return -1;
    }
    if (opts->diag != 0 && !(bench->args & BMB_ARG_DIAG)) {
        snprintf(msg, sizeof(msg), "--diag: %s has no triangular matrix to give a unit diagonal.",
                 bench->routine_name);
        bmb_log_error(msg);
        return -1;
    }
    bmb_uplo = opts->uplo != 0 ? opts->uplo : 'U';
    bmb_side = opts->side != 0 ? opts->side : 'L';
    bmb_diag = opts->diag != 0 ? opts->diag : 'N';
    if ((opts->batch_count != 0 || opts->batch_mode >= 0) && !(bench->args & BMB_ARG_BATCH)) {
        snprintf(msg, sizeof(msg), "--batch-count, --batch-mode: %s has no batched form; GEMM has.",
                 bench->routine_name);
        bmb_log_error(msg);
        return -1;
    }
    bmb_batch_count = opts->batch_count != 0 ? opts->batch_count : 1;
    bmb_batch_mode = opts->batch_mode >= 0 ? opts->batch_mode : BMB_BATCH_LOOP;
    if ((bench->args & BMB_ARG_BATCH) && !(bench->batch_modes & (1u << bmb_batch_mode))) {
        snprintf(msg, sizeof(msg), "--batch-mode %s: this build's %s has no %s call; nothing measured.",
                 bmb_batch_mode_name(bmb_batch_mode), bmb_build_backend(),
                 bmb_batch_mode == BMB_BATCH_GROUP ? "cblas_?gemm_batch" : "cblas_?gemm_batch_strided");
        bmb_log_warning(msg);
        return BMB_EXIT_SKIPPED;
    }
    return 0;
}

size_t bmb_bench_matrix_bytes(size_t rows, size_t cols, size_t size)
{
    if (rows > SIZE_MAX / size / cols) {
        return 0;
    }
    return rows * cols * size;
}

/* --align and --ld-pad (#163), set once a run. */
static size_t bmb_align = 64;
static size_t bmb_ld_pad;

void *bmb_bench_alloc(size_t bytes)
{
    void *p = NULL;

    return (posix_memalign(&p, bmb_align, bytes) == 0) ? p : NULL;
}

/* The parser keeps a size plus --ld-pad within the int BLAS takes (see
 * BMB_MAX_LD_PAD), so only the bytes can wrap. */
size_t bmb_bench_ld(size_t outer, size_t inner, size_t size)
{
    const size_t ld = inner + bmb_ld_pad;

    return (bmb_bench_matrix_bytes(outer, ld, size) != 0) ? ld : 0;
}

void bmb_bench_too_large(void)
{
    bmb_log_error("These matrices would take more bytes than size_t can count.");
}

static void bmb_warn_unused_options(const bmb_benchmark_t *bench, const bmb_options_t *opts)
{
    char msg[256];

    if (opts->batch > 1 && bench->reset_every_call) {
        snprintf(msg, sizeof(msg),
                 "--batch is ignored for %s: its operand is restored before every call, "
                 "so it is timed one call at a time.", bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->vector_size_set && !bench->use_vector_range) {
        snprintf(msg, sizeof(msg),
                 "--vector-size is ignored for %s: it takes its size from --matrix-dim1.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->matrix_dim1_set && bench->use_vector_range) {
        snprintf(msg, sizeof(msg),
                 "--matrix-dim1 is ignored for %s: it takes its size from --vector-size.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->layout_set && bench->use_vector_range) {
        snprintf(msg, sizeof(msg), "--layout is ignored for %s: it has no matrix.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->ld_pad_set && bench->use_vector_range) {
        snprintf(msg, sizeof(msg), "--ld-pad is ignored for %s: it has no matrix.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->ld_pad_set && bench->packed) {
        snprintf(msg, sizeof(msg),
                 "--ld-pad is ignored for %s: its matrix is packed, with no leading dimension.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
    if (opts->matrix_dim2_set && bench->dim2_label == NULL) {
        snprintf(msg, sizeof(msg),
                 "--matrix-dim2 is ignored for %s: it has a single size dimension (%s).",
                 bench->routine_name, bench->dim1_label);
        bmb_log_warning(msg);
    }
    if (opts->matrix_dim3_set && bench->dim3_label == NULL) {
        snprintf(msg, sizeof(msg), "--matrix-dim3 is ignored for %s: only GEMM has a K apart.",
                 bench->routine_name);
        bmb_log_warning(msg);
    }
}

int bmb_benchmark_main(int argc, char *argv[], const bmb_benchmark_t *bench)
{
    bmb_options_t opts;
    bmb_result_set_t rs;
    size_t ti;
    int status = EXIT_SUCCESS;
    char advice[512];
    const char *counts = getenv("BMB_COUNTS");

    /* Freed on the early exits too: -o may already have been parsed by the
     * time -h, -V or a bad option is reached, and leaving it allocated makes
     * every run under a leak checker report a finding that is not one. */
    switch (bmb_options_parse(argc, argv, &opts)) {
    case BMB_OPTIONS_HELP:
    case BMB_OPTIONS_VERSION:
        bmb_options_free(&opts);
        return EXIT_SUCCESS;
    case BMB_OPTIONS_ERROR:
        bmb_options_free(&opts);
        return EXIT_FAILURE;
    case BMB_OPTIONS_OK:
    default:
        break;
    }

    if (bench->level == 3 && !opts.matrix_dim1_set) {
        bmb_range_set_single(&opts.matrix_dim1, BMB_DEFAULT_LEVEL3_SIZE);
    }
    bmb_counts_only = counts != NULL && counts[0] != '\0' && counts[0] != '0';
    bmb_warn_unused_options(bench, &opts);
    bmb_threads_resolve(&opts);
    {
        int checked = bmb_check_args(bench, &opts);

        if (checked != 0) {
            bmb_options_free(&opts);
            return checked == BMB_EXIT_SKIPPED ? BMB_EXIT_SKIPPED : EXIT_FAILURE;
        }
    }
    if (opts.verify && bench->verify == NULL) {
        bmb_log_error("--verify: this benchmark has no check of its result.");
        bmb_options_free(&opts);
        return EXIT_FAILURE;
    }
    if (opts.cache_cold) {
        /* Without the size of what to push the operands out of, or of the
         * operands, a cold run would be a guess: refused, not guessed. */
        bmb_llc = bmb_machine_last_level_cache(bmb_machine());
        if (bmb_llc == 0) {
            bmb_log_error("--cache cold: the size of the last-level cache cannot be read here "
                          "(/sys/devices/system/cpu/cpu0/cache), and the copies of the operands "
                          "are counted from it.");
            bmb_options_free(&opts);
            return EXIT_FAILURE;
        }
        if (bench->footprint == NULL && bench->bytes == NULL) {
            bmb_log_error("--cache cold: this benchmark does not say how many bytes its operands take.");
            bmb_options_free(&opts);
            return EXIT_FAILURE;
        }
    }

    /* A warning, never a refusal: a laptop still gets its numbers, and the
     * provenance says under what conditions they were measured. */
    bmb_machine_frequency_advice(bmb_machine(), advice, sizeof(advice));
    if (advice[0] != '\0') {
        bmb_log_warning(advice);
    }
    bmb_threads_check_usable(&opts, bmb_machine()->usable_cpus);

    bmb_result_set_init(&rs, bench->routine_name, bench->dim1_label, bench->dim2_label,
                        opts.statistics, bench->flops != NULL, bench->bytes != NULL);
    if (bench->dim3_label != NULL && opts.matrix_dim3_set) {
        rs.dim1_label = bench->dim1_label_dim3;
        rs.dim3_label = bench->dim3_label;
    }
    rs.args = bench->args;
    memcpy(rs.trans, bmb_trans, sizeof(rs.trans));
    if (!(bench->args & BMB_ARG_TRANS2)) {
        rs.trans[1] = '\0';
    }
    rs.uplo = bmb_uplo;
    rs.side = bmb_side;
    rs.diag = bmb_diag;
    rs.batch_count = bmb_batch_count;
    rs.batch_mode = bmb_batch_mode;
    rs.label = opts.label;
    rs.verified = opts.verify;
    rs.argc = argc;
    rs.argv = argv;
    rs.warmup = opts.warmup;
    rs.iterations = opts.iterations;
    /* The batch size used, not the one asked for: -b does not batch a
     * routine whose operand is restored before every call. */
    rs.batch = bench->reset_every_call ? 1 : opts.batch;
    rs.cache_cold = opts.cache_cold;
    rs.align = opts.align;
    rs.ld_pad = bench->packed ? 0 : opts.ld_pad;
    rs.packed = bench->packed;
    rs.warmup_time = opts.warmup_time;
    rs.min_time = opts.min_time;
    rs.max_spread = opts.max_spread;
    rs.max_time = opts.max_time;
    bmb_align = opts.align;
    bmb_ld_pad = opts.ld_pad;
    if (!bench->use_vector_range) {
        rs.layout = opts.column_major ? "col" : "row";
        bmb_column_major = opts.column_major;
    }
    bmb_print_txt_begin(stdout, &rs);

    for (ti = 0; ti < opts.thread_count.count; ti++) {
        unsigned int thread_count = (unsigned int) opts.thread_count.values[ti];
        int r;

        bmb_warm_pending = opts.warmup_time > 0.0;
        r = bmb_sweep_dim1(bench, &opts, thread_count, &rs);

        if (r == -2) {
            status = BMB_EXIT_WRONG_RESULT;
            break;
        }
        if (r != 0) {
            status = EXIT_FAILURE;
        }
    }

    /* After a wrong result, no file: it would be read as a complete set
     * of verified results. */
    if (status != BMB_EXIT_WRONG_RESULT && bmb_print_save(&rs, &opts) != 0) {
        status = EXIT_FAILURE;
    }
    if (bmb_print_check_stream(stdout, "the results to stdout") != 0) {
        status = EXIT_FAILURE;
    }

    bmb_result_set_free(&rs);
    bmb_options_free(&opts);

    return status;
}
