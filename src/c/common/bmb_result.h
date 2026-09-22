#ifndef BMB_RESULT_H
#define BMB_RESULT_H

#include <stddef.h>

typedef struct {
    unsigned int thread_count;
    size_t dim1;
    size_t dim2; /* only meaningful when the result set's dim2_label is non-NULL */
    double time_s;
    double gflops;     /* 0 when the routine declares no operation count */
    double gbytes_s;   /* 0 when the routine declares no memory traffic */
    double stddev_s;
    double min_s;
    double max_s;
} bmb_result_row_t;

typedef struct {
    const char *routine_name;
    const char *dim1_label; /* e.g. "Vector size", "Matrix dim (N)", "Matrix dim1 (M)" */
    const char *dim2_label; /* NULL if the routine has a single size dimension */
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
int bmb_result_set_add(bmb_result_set_t *rs, bmb_result_row_t row);
void bmb_result_set_free(bmb_result_set_t *rs);

#endif /* BMB_RESULT_H */
