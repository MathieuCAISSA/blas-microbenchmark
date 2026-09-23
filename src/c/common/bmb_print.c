#include "bmb_print.h"
#include "bmb_build.h"
#include "bmb_log.h"

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

void bmb_print_txt_begin(FILE *out, const bmb_result_set_t *rs)
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
        fprintf(out, "%-16s%-16s%-16s%-14s", "mean [s]", "stddev [s]", "max [s]",
                "calls/sample");
    }
    fprintf(out, "\n");
}

void bmb_print_txt_row(FILE *out, const bmb_result_set_t *rs,
                        const bmb_result_row_t *row)
{
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
        fprintf(out, "%-16.9f%-16.9f%-16.9f%-14u", row->mean_s, row->stddev_s, row->max_s,
                row->batch);
    }
    fprintf(out, "\n");

    /* Flushed per row so that progress is visible through a pipe or a
     * redirect, where stdout is block-buffered and would otherwise show
     * nothing until the sweep ends. One flush per measurement is free next
     * to the measurement itself. */
    fflush(out);
}

static void bmb_csv_header_label(FILE *out, const char *label)
{
    /* CSV column names: lowercase, every run of other characters collapsed
     * to one underscore, e.g. "Matrix dim1 (M=K)" -> "matrix_dim1_m_k".
     * The separator is held back until another word character turns up, so
     * a label ending in punctuation does not leave a trailing underscore
     * on the column name. */
    const char *p;
    int pending_sep = 0;
    int wrote_any = 0;

    for (p = label; *p != '\0'; p++) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
            if (pending_sep && wrote_any) {
                fputc('_', out);
            }
            fputc((*p >= 'A' && *p <= 'Z') ? (*p - 'A' + 'a') : *p, out);
            pending_sep = 0;
            wrote_any = 1;
        } else {
            pending_sep = 1;
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
        fprintf(out, ",mean_s,stddev_s,max_s,calls_per_sample");
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
            fprintf(out, ",%.9f,%.9f,%.9f,%u", row->mean_s, row->stddev_s, row->max_s,
                    row->batch);
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
            fprintf(out, ",\n      \"mean_s\": %.9f,\n      \"stddev_s\": %.9f,"
                         "\n      \"max_s\": %.9f,\n      \"calls_per_sample\": %u",
                    row->mean_s, row->stddev_s, row->max_s, row->batch);
        }
        fprintf(out, "\n    }%s\n", (i + 1 < rs->count) ? "," : "");
    }
    fprintf(out, "  ]\n}\n");
}

int bmb_print_check_stream(FILE *stream, const char *what)
{
    char msg[256];

    if (fflush(stream) == 0 && ferror(stream) == 0) {
        return 0;
    }

    snprintf(msg, sizeof(msg), "Failed to write %s.", what);
    bmb_log_error(msg);
    return -1;
}

int bmb_print_save(const bmb_result_set_t *rs, const bmb_options_t *opts)
{
    FILE *f;
    char msg[512];
    int status = 0;

    if (opts->output_file == NULL) {
        return 0;
    }

    f = fopen(opts->output_file, "w");
    if (f == NULL) {
        snprintf(msg, sizeof(msg), "Unable to open %s for writing.", opts->output_file);
        bmb_log_error(msg);
        return -1;
    }

    if (opts->output_format == BMB_FORMAT_JSON) {
        bmb_print_json(f, rs);
    } else {
        bmb_print_csv(f, rs);
    }

    /* Both are needed: ferror catches a write that already failed, and
     * fclose is where a buffered one finally reaches the disk. */
    if (ferror(f) != 0) {
        status = -1;
    }
    if (fclose(f) != 0) {
        status = -1;
    }
    if (status != 0) {
        snprintf(msg, sizeof(msg), "Failed to write %s in full; the file is incomplete.",
                 opts->output_file);
        bmb_log_error(msg);
    }

    return status;
}
