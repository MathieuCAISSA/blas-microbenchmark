#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_print.h"
#include "bmb_result.h"
#include "bmb_threads.h"
#include "bmb_timer.h"

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

    /* Rates come from the mean. GFLOP/s and GB/s are decimal (1e9), as
     * they conventionally are for compute and bandwidth figures. */
    row->gflops = 0.0;
    row->gbytes_s = 0.0;
    if (row->time_s > 0.0) {
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

static int bmb_record(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                       unsigned int thread_count, size_t dim1, size_t dim2,
                       bmb_result_set_t *rs)
{
    bmb_result_row_t row;

    if (bmb_run_one(bench, opts, thread_count, dim1, dim2, &row) != 0) {
        return -1;
    }

    bmb_print_txt_row(stdout, rs, &row);

    /* Kept as well, because the file output needs the whole set: a failure
     * here costs the file, not the row the user has already seen. */
    if (bmb_result_set_add(rs, row) != 0) {
        bmb_log_error("Out of memory while recording results.");
        return -1;
    }

    return 0;
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

        if (bench->dim2_label != NULL) {
            size_t i2;

            for (i2 = 0; i2 < range2->count; i2++) {
                if (bmb_record(bench, opts, thread_count, d1, range2->values[i2], rs) != 0) {
                    status = -1;
                }
            }
        } else {
            if (bmb_record(bench, opts, thread_count, d1, 0, rs) != 0) {
                status = -1;
            }
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
    size_t ti;
    int status = EXIT_SUCCESS;

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

    bmb_warn_unused_options(bench, &opts);
    bmb_threads_resolve(&opts);

    bmb_result_set_init(&rs, bench->routine_name, bench->dim1_label, bench->dim2_label,
                        opts.statistics, bench->flops != NULL, bench->bytes != NULL);
    bmb_print_txt_begin(stdout, &rs);

    for (ti = 0; ti < opts.thread_count.count; ti++) {
        unsigned int thread_count = (unsigned int) opts.thread_count.values[ti];

        if (bmb_sweep_dim1(bench, &opts, thread_count, &rs) != 0) {
            status = EXIT_FAILURE;
        }
    }

    if (bmb_print_save(&rs, &opts) != 0) {
        status = EXIT_FAILURE;
    }
    if (bmb_print_check_stream(stdout, "the results to stdout") != 0) {
        status = EXIT_FAILURE;
    }

    bmb_result_set_free(&rs);
    bmb_options_free(&opts);

    return status;
}
