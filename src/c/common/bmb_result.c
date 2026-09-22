#include <stdlib.h>

#include "bmb_result.h"

void bmb_result_set_init(bmb_result_set_t *rs, const char *routine_name,
                          const char *dim1_label, const char *dim2_label,
                          int has_stats, int has_flops, int has_bytes)
{
    rs->routine_name = routine_name;
    rs->dim1_label = dim1_label;
    rs->dim2_label = dim2_label;
    rs->has_stats = has_stats;
    rs->has_flops = has_flops;
    rs->has_bytes = has_bytes;
    rs->rows = NULL;
    rs->count = 0;
    rs->capacity = 0;
}

int bmb_result_set_add(bmb_result_set_t *rs, bmb_result_row_t row)
{
    if (rs->count == rs->capacity) {
        size_t new_capacity = (rs->capacity == 0) ? 8 : rs->capacity * 2;
        bmb_result_row_t *new_rows = realloc(rs->rows, new_capacity * sizeof(*new_rows));

        if (new_rows == NULL) {
            return -1;
        }
        rs->rows = new_rows;
        rs->capacity = new_capacity;
    }

    rs->rows[rs->count++] = row;
    return 0;
}

void bmb_result_set_free(bmb_result_set_t *rs)
{
    free(rs->rows);
    rs->rows = NULL;
    rs->count = 0;
    rs->capacity = 0;
}
