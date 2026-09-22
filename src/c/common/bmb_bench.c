#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_print.h"
#include "bmb_result.h"
#include "bmb_threads.h"
#include "bmb_timer.h"

/* Next point of a sweep: doubles, except that the last step lands exactly
 * on max instead of overshooting it, so the requested endpoint is always
 * measured (100:1000 gives 100, 200, 400, 800, 1000). Callers must stop
 * once v has reached max, so this is only ever called with v < max --
 * which also means v <= max/2 whenever it doubles, so v * 2 cannot
 * overflow. */
static size_t bmb_sweep_next(size_t v, size_t max)
{
    if (v > max / 2) {
        return max;
    }
    return v * 2;
}

static void bmb_compute_stats(const double *times, unsigned int n, bmb_result_row_t *row)
{
    unsigned int i;
    double sum = 0.0;
    double variance = 0.0;

    row->min_s = times[0];
    row->max_s = times[0];
    for (i = 0; i < n; i++) {
        sum += times[i];
        if (times[i] < row->min_s) {
            row->min_s = times[i];
        }
        if (times[i] > row->max_s) {
            row->max_s = times[i];
        }
    }
    row->time_s = sum / (double) n;

    for (i = 0; i < n; i++) {
        double d = times[i] - row->time_s;
        variance += d * d;
    }
    /* Population variance (divided by n, not n-1): the n timed iterations
     * are the whole of what was measured, not a sample drawn from it. */
    variance /= (double) n;
    row->stddev_s = sqrt(variance);
}

static int bmb_run_one(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                        unsigned int thread_count, size_t dim1, size_t dim2,
                        bmb_result_row_t *row)
{
    void *ctx;
    double *times;
    unsigned int i;

    /* setup() allocates up to dim1 * dim2 doubles; refuse a product that
     * would wrap around size_t rather than let it turn into a small
     * allocation that the BLAS call then overruns. */
    if (dim2 != 0 && dim1 > (SIZE_MAX / sizeof(double)) / dim2) {
        bmb_log_error("Requested dimensions are too large to allocate; skipping this data point.");
        return -1;
    }

    bmb_threads_set(thread_count);

    ctx = bench->setup(dim1, dim2, thread_count);
    if (ctx == NULL) {
        bmb_log_error("Benchmark setup failed (out of memory?); skipping this data point.");
        return -1;
    }

    for (i = 0; i < opts->warmup; i++) {
        if (bench->reset != NULL) {
            bench->reset(ctx);
        }
        bench->call(ctx);
    }

    times = malloc(opts->iterations * sizeof(*times));
    if (times == NULL) {
        bmb_log_error("Out of memory while allocating timing buffer.");
        bench->teardown(ctx);
        return -1;
    }

    for (i = 0; i < opts->iterations; i++) {
        double t0, t1;

        /* Deliberately before t0: restoring an operand is bookkeeping, and
         * for dtrmm/dtrsm it is an O(M*N) copy that would otherwise be a
         * large part of what gets reported as the routine's time. */
        if (bench->reset != NULL) {
            bench->reset(ctx);
        }

        t0 = bmb_timer_now();
        bench->call(ctx);
        t1 = bmb_timer_now();

        times[i] = t1 - t0;
    }

    bench->teardown(ctx);

    row->thread_count = thread_count;
    row->dim1 = dim1;
    row->dim2 = dim2;
    bmb_compute_stats(times, opts->iterations, row);

    free(times);
    return 0;
}

static int bmb_record(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                       unsigned int thread_count, size_t dim1, size_t dim2,
                       bmb_result_set_t *rs)
{
    bmb_result_row_t row;

    if (bmb_run_one(bench, opts, thread_count, dim1, dim2, &row) != 0) {
        return -1;
    }
    if (bmb_result_set_add(rs, row) != 0) {
        bmb_log_error("Out of memory while recording results.");
        return -1;
    }

    return 0;
}

/* The sweeps below test for the endpoint at the bottom of the loop rather
 * than in the for condition: bmb_sweep_next() stops exactly on max, so a
 * top-of-loop `v <= max` test would never terminate. */
static int bmb_sweep_dim1(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                           unsigned int thread_count, bmb_result_set_t *rs)
{
    bmb_range_t range1 = bench->use_vector_range ? opts->vector_size : opts->matrix_dim1;
    bmb_range_t range2 = opts->matrix_dim2;
    size_t d1;
    int status = 0;

    for (d1 = range1.min;; d1 = bmb_sweep_next(d1, range1.max)) {
        if (bench->dim2_label != NULL) {
            size_t d2;

            for (d2 = range2.min;; d2 = bmb_sweep_next(d2, range2.max)) {
                if (bmb_record(bench, opts, thread_count, d1, d2, rs) != 0) {
                    status = -1;
                }
                if (d2 >= range2.max) {
                    break;
                }
            }
        } else {
            if (bmb_record(bench, opts, thread_count, d1, 0, rs) != 0) {
                status = -1;
            }
        }

        if (d1 >= range1.max) {
            break;
        }
    }

    return status;
}

/* A size option that this particular routine has no use for would
 * otherwise be accepted and quietly dropped, leaving the user reading
 * numbers for a size they never asked for. */
static void bmb_warn_unused_options(const bmb_benchmark_t *bench, const bmb_options_t *opts)
{
    char msg[256];

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
    if (opts->matrix_dim2_set && bench->dim2_label == NULL) {
        snprintf(msg, sizeof(msg),
                 "--matrix-dim2 is ignored for %s: it has a single size dimension (%s).",
                 bench->routine_name, bench->dim1_label);
        bmb_log_warning(msg);
    }
}

int bmb_benchmark_main(int argc, char *argv[], const bmb_benchmark_t *bench)
{
    bmb_options_t opts;
    bmb_result_set_t rs;
    unsigned int thread_count;
    int status = EXIT_SUCCESS;

    switch (bmb_options_parse(argc, argv, &opts)) {
    case BMB_OPTIONS_HELP:
        return EXIT_SUCCESS;
    case BMB_OPTIONS_ERROR:
        return EXIT_FAILURE;
    case BMB_OPTIONS_OK:
    default:
        break;
    }

    bmb_warn_unused_options(bench, &opts);
    bmb_threads_resolve(&opts);

    bmb_result_set_init(&rs, bench->routine_name, bench->dim1_label, bench->dim2_label, opts.statistics);

    for (thread_count = (unsigned int) opts.thread_count.min;;
         thread_count = (unsigned int) bmb_sweep_next(thread_count, opts.thread_count.max)) {

        if (bmb_sweep_dim1(bench, &opts, thread_count, &rs) != 0) {
            status = EXIT_FAILURE;
        }

        if (thread_count >= opts.thread_count.max) {
            break;
        }
    }

    bmb_print_results(&rs, &opts);

    bmb_result_set_free(&rs);
    bmb_options_free(&opts);

    return status;
}
