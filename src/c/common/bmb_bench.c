#include <math.h>
#include <stdlib.h>

#include "bmb_bench.h"
#include "bmb_log.h"
#include "bmb_print.h"
#include "bmb_result.h"
#include "bmb_threads.h"
#include "bmb_timer.h"

static size_t bmb_sweep_next(size_t v, size_t max)
{
    if (v > max / 2) {
        return max + 1; /* stop the loop */
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
        bench->call(ctx);
    }

    times = malloc(opts->iterations * sizeof(*times));
    if (times == NULL) {
        bmb_log_error("Out of memory while allocating timing buffer.");
        bench->teardown(ctx);
        return -1;
    }

    for (i = 0; i < opts->iterations; i++) {
        double t0 = bmb_timer_now();
        bench->call(ctx);
        double t1 = bmb_timer_now();

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

static int bmb_sweep_dim1(const bmb_benchmark_t *bench, const bmb_options_t *opts,
                           unsigned int thread_count, bmb_result_set_t *rs)
{
    bmb_range_t range1 = bench->use_vector_range ? opts->vector_size : opts->matrix_dim1;
    size_t d1;
    int status = 0;

    for (d1 = range1.min; d1 <= range1.max; d1 = bmb_sweep_next(d1, range1.max)) {
        if (bench->dim2_label != NULL) {
            size_t d2;

            for (d2 = opts->matrix_dim2.min; d2 <= opts->matrix_dim2.max;
                 d2 = bmb_sweep_next(d2, opts->matrix_dim2.max)) {
                bmb_result_row_t row;

                if (bmb_run_one(bench, opts, thread_count, d1, d2, &row) == 0) {
                    if (bmb_result_set_add(rs, row) != 0) {
                        bmb_log_error("Out of memory while recording results.");
                        status = -1;
                    }
                } else {
                    status = -1;
                }
            }
        } else {
            bmb_result_row_t row;

            if (bmb_run_one(bench, opts, thread_count, d1, 0, &row) == 0) {
                if (bmb_result_set_add(rs, row) != 0) {
                    bmb_log_error("Out of memory while recording results.");
                    status = -1;
                }
            } else {
                status = -1;
            }
        }
    }

    return status;
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

    bmb_threads_resolve(&opts);

    bmb_result_set_init(&rs, bench->routine_name, bench->dim1_label, bench->dim2_label, opts.statistics);

    for (thread_count = (unsigned int) opts.thread_count.min;
         thread_count <= (unsigned int) opts.thread_count.max;
         thread_count = (unsigned int) bmb_sweep_next(thread_count, opts.thread_count.max)) {

        if (bmb_sweep_dim1(bench, &opts, thread_count, &rs) != 0) {
            status = EXIT_FAILURE;
        }
    }

    bmb_print_results(&rs, &opts);

    bmb_result_set_free(&rs);
    bmb_options_free(&opts);

    return status;
}
