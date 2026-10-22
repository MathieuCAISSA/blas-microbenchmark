#ifndef BMB_RESULT_H
#define BMB_RESULT_H

#include <stddef.h>

typedef struct {
    unsigned int thread_count;
    size_t dim1;
    size_t dim2; /* only meaningful when the result set's dim2_label is non-NULL */
    /* The fastest sample, which is the headline figure: each sample is
     * already a mean over `batch` calls, and the fastest one is the least
     * contaminated by whatever else the machine was doing. */
    double time_s;
    unsigned int batch;  /* calls averaged into each sample */
    double gflops;       /* 0 when the routine declares no operation count */
    double gbytes_s;     /* 0 when the routine declares no memory traffic */
    /* The spread across samples, shown by -s. */
    double mean_s;
    double stddev_s;
    double max_s;
    /* The samples' median and quartiles, always written (#161): the
     * fastest sample says how fast the routine can go, these how steady
     * that was. */
    double median_s;
    double q1_s;
    double q3_s;
    unsigned int copies; /* --cache cold: the copies of the operands rotated over */
} bmb_result_row_t;

typedef struct {
    const char *routine_name;
    const char *dim1_label; /* e.g. "Vector size", "Matrix dim (N)", "Matrix dim1 (M)" */
    const char *dim2_label; /* NULL if the routine has a single size dimension */
    const char *label;      /* --label, or NULL; owned by the options */
    int verified;           /* --verify: every point was checked */
    const char *layout;     /* "row" or "col" for routines with a matrix;
                             * NULL for vector routines, which have none */
    /* How the run was made, for the files to say (#161). argv is main's,
     * argc 0 when there is none; batch is the size every point used, 0
     * when it was chosen per point. */
    int argc;
    char *const *argv;
    unsigned int warmup;
    unsigned int iterations;
    unsigned int batch;
    int cache_cold;         /* --cache cold (#160) */
    int has_stats;
    int has_flops;
    int has_bytes;
    bmb_result_row_t *rows;
    size_t count;
    size_t capacity;
} bmb_result_set_t;

void bmb_result_set_init(bmb_result_set_t *rs, const char *routine_name,
                          const char *dim1_label, const char *dim2_label,
                          int has_stats, int has_flops, int has_bytes);
int bmb_result_set_add(bmb_result_set_t *rs, const bmb_result_row_t *row);
void bmb_result_set_free(bmb_result_set_t *rs);

#endif /* BMB_RESULT_H */
