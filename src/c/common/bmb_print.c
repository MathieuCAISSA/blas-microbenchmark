#include <stdarg.h>

#include "bmb_print.h"
#include "bmb_log.h"

void bmb_print_format(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

static void bmb_print_txt_header(FILE *out, const bmb_result_set_t *rs)
{
    fprintf(out, "# routine: %s\n", rs->routine_name);
    fprintf(out, "%-16s", "Thread count");
    fprintf(out, "%-16s", rs->dim1_label);
    if (rs->dim2_label != NULL) {
        fprintf(out, "%-16s", rs->dim2_label);
    }
    fprintf(out, "%-14s", "time [s]");
    if (rs->has_stats) {
        fprintf(out, "%-14s%-14s%-14s", "stddev [s]", "min [s]", "max [s]");
    }
    fprintf(out, "\n");
}

void bmb_print_txt(FILE *out, const bmb_result_set_t *rs)
{
    size_t i;

    bmb_print_txt_header(out, rs);
    for (i = 0; i < rs->count; i++) {
        const bmb_result_row_t *row = &rs->rows[i];

        fprintf(out, "%-16u", row->thread_count);
        fprintf(out, "%-16zu", row->dim1);
        if (rs->dim2_label != NULL) {
            fprintf(out, "%-16zu", row->dim2);
        }
        fprintf(out, "%-14.6f", row->time_s);
        if (rs->has_stats) {
            fprintf(out, "%-14.6f%-14.6f%-14.6f", row->stddev_s, row->min_s, row->max_s);
        }
        fprintf(out, "\n");
    }
}

static void bmb_csv_header_label(FILE *out, const char *label)
{
    /* CSV column names: lowercase, spaces/parens stripped down to a
     * single identifier-like token, e.g. "Matrix dim1 (M)" -> "matrix_dim1_m". */
    const char *p;
    int last_was_sep = 0;

    for (p = label; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
            fputc((*p >= 'A' && *p <= 'Z') ? (*p - 'A' + 'a') : *p, out);
            last_was_sep = 0;
        } else if (!last_was_sep) {
            fputc('_', out);
            last_was_sep = 1;
        }
    }
}

void bmb_print_csv(FILE *out, const bmb_result_set_t *rs)
{
    size_t i;

    fprintf(out, "thread_count,");
    bmb_csv_header_label(out, rs->dim1_label);
    if (rs->dim2_label != NULL) {
        fprintf(out, ",");
        bmb_csv_header_label(out, rs->dim2_label);
    }
    fprintf(out, ",time_s");
    if (rs->has_stats) {
        fprintf(out, ",stddev_s,min_s,max_s");
    }
    fprintf(out, "\n");

    for (i = 0; i < rs->count; i++) {
        const bmb_result_row_t *row = &rs->rows[i];

        fprintf(out, "%u,", row->thread_count);
        fprintf(out, "%zu", row->dim1);
        if (rs->dim2_label != NULL) {
            fprintf(out, ",%zu", row->dim2);
        }
        fprintf(out, ",%.6f", row->time_s);
        if (rs->has_stats) {
            fprintf(out, ",%.6f,%.6f,%.6f", row->stddev_s, row->min_s, row->max_s);
        }
        fprintf(out, "\n");
    }
}

void bmb_print_json(FILE *out, const bmb_result_set_t *rs)
{
    size_t i;

    fprintf(out, "{\n  \"routine\": \"%s\",\n  \"results\": [\n", rs->routine_name);
    for (i = 0; i < rs->count; i++) {
        const bmb_result_row_t *row = &rs->rows[i];

        fprintf(out, "    {\n");
        fprintf(out, "      \"thread_count\": %u,\n", row->thread_count);
        fprintf(out, "      \"dim1\": %zu", row->dim1);
        if (rs->dim2_label != NULL) {
            fprintf(out, ",\n      \"dim2\": %zu", row->dim2);
        }
        fprintf(out, ",\n      \"time_s\": %.6f", row->time_s);
        if (rs->has_stats) {
            fprintf(out, ",\n      \"stddev_s\": %.6f,\n      \"min_s\": %.6f,\n      \"max_s\": %.6f",
                    row->stddev_s, row->min_s, row->max_s);
        }
        fprintf(out, "\n    }%s\n", (i + 1 < rs->count) ? "," : "");
    }
    fprintf(out, "  ]\n}\n");
}

void bmb_print_results(const bmb_result_set_t *rs, const bmb_options_t *opts)
{
    FILE *f;

    bmb_print_txt(stdout, rs);

    if (opts->output_file == NULL) {
        return;
    }

    f = fopen(opts->output_file, "w");
    if (f == NULL) {
        bmb_log_error("Unable to open output file for writing.");
        return;
    }

    if (opts->output_format == BMB_FORMAT_JSON) {
        bmb_print_json(f, rs);
    } else {
        bmb_print_csv(f, rs);
    }

    fclose(f);
}
