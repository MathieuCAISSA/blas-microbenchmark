#include <stdarg.h>

#include "bmb_print.h"
#include "bmb_build.h"
#include "bmb_log.h"

void bmb_print_format(const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vfprintf(stdout, format, args);
    va_end(args);
}

/* Which build and which BLAS produced the numbers below. Written as
 * comment lines so the same two lines can head the text and the CSV
 * output; a CSV reader is told to skip them (pandas: comment="#"). A
 * results file that does not name the library it measured is close to
 * useless a month later, which is a pity for a project whose purpose is
 * comparing libraries. */
static void bmb_print_provenance(FILE *out)
{
    const char *blas = bmb_build_blas_version();

    fprintf(out, "# blas-microbenchmark %s\n", bmb_build_version());
    if (blas != NULL && blas[0] != '\0') {
        fprintf(out, "# backend: %s (%s)\n", bmb_build_backend(), blas);
    } else {
        fprintf(out, "# backend: %s\n", bmb_build_backend());
    }
}

static void bmb_print_txt_header(FILE *out, const bmb_result_set_t *rs)
{
    bmb_print_provenance(out);
    fprintf(out, "# routine: %s\n", rs->routine_name);
    fprintf(out, "%-16s", "Thread count");
    fprintf(out, "%-20s", rs->dim1_label);
    if (rs->dim2_label != NULL) {
        fprintf(out, "%-20s", rs->dim2_label);
    }
    fprintf(out, "%-16s", "time [s]");
    if (rs->has_flops) {
        fprintf(out, "%-14s", "GFLOP/s");
    }
    if (rs->has_bytes) {
        fprintf(out, "%-14s", "GB/s");
    }
    if (rs->has_stats) {
        fprintf(out, "%-16s%-16s%-16s", "stddev [s]", "min [s]", "max [s]");
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
        fprintf(out, "%-20zu", row->dim1);
        if (rs->dim2_label != NULL) {
            fprintf(out, "%-20zu", row->dim2);
        }
        fprintf(out, "%-16.9f", row->time_s);
        if (rs->has_flops) {
            fprintf(out, "%-14.3f", row->gflops);
        }
        if (rs->has_bytes) {
            fprintf(out, "%-14.3f", row->gbytes_s);
        }
        if (rs->has_stats) {
            fprintf(out, "%-16.9f%-16.9f%-16.9f", row->stddev_s, row->min_s, row->max_s);
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

    bmb_print_provenance(out);
    fprintf(out, "# routine: %s\n", rs->routine_name);
    fprintf(out, "thread_count,");
    bmb_csv_header_label(out, rs->dim1_label);
    if (rs->dim2_label != NULL) {
        fprintf(out, ",");
        bmb_csv_header_label(out, rs->dim2_label);
    }
    fprintf(out, ",time_s");
    if (rs->has_flops) {
        fprintf(out, ",gflops");
    }
    if (rs->has_bytes) {
        fprintf(out, ",gbytes_per_s");
    }
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
        fprintf(out, ",%.9f", row->time_s);
        if (rs->has_flops) {
            fprintf(out, ",%.6f", row->gflops);
        }
        if (rs->has_bytes) {
            fprintf(out, ",%.6f", row->gbytes_s);
        }
        if (rs->has_stats) {
            fprintf(out, ",%.9f,%.9f,%.9f", row->stddev_s, row->min_s, row->max_s);
        }
        fprintf(out, "\n");
    }
}

/* The BLAS version string comes from the library, not from us, so it gets
 * escaped rather than trusted to be JSON-safe. */
static void bmb_print_json_string(FILE *out, const char *value)
{
    const char *p;

    fputc('"', out);
    for (p = value; *p != '\0'; p++) {
        if (*p == '"' || *p == '\\') {
            fprintf(out, "\\%c", *p);
        } else if ((unsigned char) *p < 0x20) {
            fprintf(out, "\\u%04x", (unsigned char) *p);
        } else {
            fputc(*p, out);
        }
    }
    fputc('"', out);
}

void bmb_print_json(FILE *out, const bmb_result_set_t *rs)
{
    const char *blas = bmb_build_blas_version();
    size_t i;

    fprintf(out, "{\n");
    fprintf(out, "  \"version\": \"%s\",\n", bmb_build_version());
    fprintf(out, "  \"backend\": \"%s\",\n", bmb_build_backend());
    if (blas != NULL && blas[0] != '\0') {
        fprintf(out, "  \"blas\": ");
        bmb_print_json_string(out, blas);
        fprintf(out, ",\n");
    }
    fprintf(out, "  \"routine\": \"%s\",\n  \"results\": [\n", rs->routine_name);
    for (i = 0; i < rs->count; i++) {
        const bmb_result_row_t *row = &rs->rows[i];

        fprintf(out, "    {\n");
        fprintf(out, "      \"thread_count\": %u,\n", row->thread_count);
        fprintf(out, "      \"dim1\": %zu", row->dim1);
        if (rs->dim2_label != NULL) {
            fprintf(out, ",\n      \"dim2\": %zu", row->dim2);
        }
        fprintf(out, ",\n      \"time_s\": %.9f", row->time_s);
        if (rs->has_flops) {
            fprintf(out, ",\n      \"gflops\": %.6f", row->gflops);
        }
        if (rs->has_bytes) {
            fprintf(out, ",\n      \"gbytes_per_s\": %.6f", row->gbytes_s);
        }
        if (rs->has_stats) {
            fprintf(out, ",\n      \"stddev_s\": %.9f,\n      \"min_s\": %.9f,\n      \"max_s\": %.9f",
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
