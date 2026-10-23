#include <config.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_print.h"
#include "bmb_build.h"
#include "bmb_machine.h"
#include "bmb_log.h"

/* The variables that change what a BLAS library does -- its threads,
 * where they run, which kernels it picks -- recorded when set (#161): a
 * result measured under OMP_PROC_BIND=close is not one measured without.
 * In the order a reader looks for them: OpenMP's, then each library's. */
static const char *const bmb_run_variables[] = {
    "OMP_NUM_THREADS", "OMP_PROC_BIND", "OMP_PLACES", "OMP_DYNAMIC", "OMP_WAIT_POLICY",
    "GOMP_CPU_AFFINITY", "GOMP_SPINCOUNT", "KMP_AFFINITY", "KMP_BLOCKTIME", "KMP_HW_SUBSET",
    "OPENBLAS_NUM_THREADS", "GOTO_NUM_THREADS", "OPENBLAS_CORETYPE",
    "BLIS_NUM_THREADS", "BLIS_JC_NT", "BLIS_PC_NT", "BLIS_IC_NT", "BLIS_JR_NT", "BLIS_IR_NT",
    "BLIS_ARCH_TYPE", "AOCL_ENABLE_INSTRUCTIONS",
    "MKL_NUM_THREADS", "MKL_DOMAIN_NUM_THREADS", "MKL_DYNAMIC", "MKL_THREADING_LAYER",
    "MKL_ENABLE_INSTRUCTIONS", "MKL_CBWR",
    "FLEXIBLAS"
};

#define BMB_RUN_VARIABLES (sizeof(bmb_run_variables) / sizeof(bmb_run_variables[0]))

/* A word of the command line or a variable's value, as a shell would take
 * it back: bare when it is plain, in single quotes otherwise. A control
 * character becomes '?', so that a file name holding a newline cannot end
 * the comment line it is written on. */
static void bmb_print_shell_word(FILE *out, const char *word)
{
    const char *p;
    int plain = word[0] != '\0';

    for (p = word; *p != '\0' && plain; p++) {
        plain = isalnum((unsigned char) *p) || strchr("_-./:=,+@%", *p) != NULL;
    }
    if (plain) {
        fputs(word, out);
        return;
    }
    fputc('\'', out);
    for (p = word; *p != '\0'; p++) {
        if (*p == '\'') {
            fputs("'\\''", out);
        } else if ((unsigned char) *p < 0x20 || *p == 0x7f) {
            fputc('?', out);
        } else {
            fputc(*p, out);
        }
    }
    fputc('\'', out);
}

/* The program by its name alone: the directory it was run from says
 * nothing about the result. */
static const char *bmb_print_program(const char *argv0)
{
    const char *slash = strrchr(argv0, '/');

    return (slash != NULL) ? slash + 1 : argv0;
}

/* Whether -T or -e asked for more samples than -i (#164): the samples
 * taken then go in the table too. */
static int bmb_print_adaptive(const bmb_result_set_t *rs)
{
    return rs->min_time > 0.0 || rs->max_spread > 0.0;
}

/* How the run was made, after where it ran: everything needed to make
 * the same measurement again (#161). */
static void bmb_print_run(FILE *out, const bmb_result_set_t *rs, const bmb_machine_t *m)
{
    size_t i;
    int first = 1;

    if (m->cpus_allowed[0] != '\0') {
        fprintf(out, "# affinity: CPUs %s\n", m->cpus_allowed);
    }
    if (rs->argc > 0) {
        int a;

        fprintf(out, "# command: ");
        bmb_print_shell_word(out, bmb_print_program(rs->argv[0]));
        for (a = 1; a < rs->argc; a++) {
            fputc(' ', out);
            bmb_print_shell_word(out, rs->argv[a]);
        }
        fputc('\n', out);
    }
    fprintf(out, "# run: %g s and %u call%s of warm-up, %u sample%s", rs->warmup_time, rs->warmup,
            rs->warmup == 1 ? "" : "s", rs->iterations, rs->iterations == 1 ? "" : "s");
    if (bmb_print_adaptive(rs)) {
        fprintf(out, " or more, for %g s", rs->min_time);
        if (rs->max_spread > 0.0) {
            fprintf(out, " and an IQR of %g %%", rs->max_spread);
        }
        fprintf(out, ", at most %g s", rs->max_time);
    }
    fprintf(out, ", ");
    if (rs->batch != 0) {
        fprintf(out, "batches of %u call%s, ", rs->batch, rs->batch == 1 ? "" : "s");
    } else {
        fprintf(out, "batch size chosen per point, ");
    }
    fprintf(out, "%s cache\n", rs->cache_cold ? "cold" : "hot");
    fprintf(out, "# operands: aligned to %u bytes", rs->align);
    if (rs->layout != NULL) {
        fprintf(out, ", leading dimensions + %u", rs->ld_pad);
    }
    fputc('\n', out);
    fprintf(out, "# compiler: %s, CFLAGS %s\n", bmb_build_compiler(), bmb_build_cflags());
    for (i = 0; i < BMB_RUN_VARIABLES; i++) {
        const char *value = getenv(bmb_run_variables[i]);

        if (value != NULL) {
            fprintf(out, "%s%s=", first ? "# environment: " : " ", bmb_run_variables[i]);
            bmb_print_shell_word(out, value);
            first = 0;
        }
    }
    if (!first) {
        fputc('\n', out);
    }
}

/* Which build and which BLAS produced the numbers below. Written as
 * comment lines so the same two lines can head the text and the CSV
 * output; a CSV reader is told to skip them (pandas: comment="#"). A
 * results file that does not name the library it measured is close to
 * useless a month later, which is a pity for a project whose purpose is
 * comparing libraries. */
static void bmb_print_provenance(FILE *out, const bmb_result_set_t *rs)
{
    const char *blas = bmb_build_blas_version();
    const bmb_machine_t *m = bmb_machine();
    char line[512];

    fprintf(out, "# blas-microbenchmark %s\n", bmb_build_version());
    if (blas != NULL && blas[0] != '\0') { /* GCOVR_EXCL_BR_LINE: no library this builds against reports an empty version */
        fprintf(out, "# backend: %s (%s)\n", bmb_build_backend(), blas);
    } else {
        fprintf(out, "# backend: %s\n", bmb_build_backend());
    }
#if defined(BMB_BUILD_HAS_KERNELS)
    fprintf(out, "# kernels: %s\n", bmb_build_kernels());
#endif
    if (rs->label != NULL) {
        fprintf(out, "# label: %s\n", rs->label);
    }
    if (rs->verified) {
        fprintf(out, "# verified: every point, against a reference, before it was timed\n");
    }
    if (rs->layout != NULL) {
        fprintf(out, "# layout: %s\n", strcmp(rs->layout, "col") == 0 ? "column-major" : "row-major");
    }

    bmb_machine_describe_cpu(m, line, sizeof(line));
    if (line[0] != '\0') { /* GCOVR_EXCL_BR_LINE: sysconf() always gives the CPU count on Linux */
        fprintf(out, "# cpu: %s\n", line);
    }
    bmb_machine_describe_caches(m, line, sizeof(line));
    if (line[0] != '\0') {
        fprintf(out, "# caches: %s\n", line);
    }
    bmb_machine_describe_frequency(m, line, sizeof(line));
    if (line[0] != '\0') {
        fprintf(out, "# frequency: %s\n", line);
    }
    if (m->os[0] != '\0') { /* GCOVR_EXCL_BR_LINE: uname() cannot fail with a valid buffer */
        fprintf(out, "# os: %s\n", m->os);
    }
    if (m->date[0] != '\0') { /* GCOVR_EXCL_BR_LINE: time() and gmtime_r() of now do not fail */
        fprintf(out, "# date: %s\n", m->date);
    }
    bmb_print_run(out, rs, m);
}

void bmb_print_txt_begin(FILE *out, const bmb_result_set_t *rs)
{
    bmb_print_provenance(out, rs);
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
    /* -s's columns first, where they always were, so that a table or a
     * CSV read by column position keeps its columns (#161). */
    if (rs->has_stats) {
        fprintf(out, "%-16s%-16s%-16s%-14s", "mean [s]", "stddev [s]", "max [s]", "calls/sample");
    }
    fprintf(out, "%-16s%-10s", "median [s]", "IQR [%]");
    if (bmb_print_adaptive(rs)) {
        fprintf(out, "%-10s", "samples");
    }
    if (rs->cache_cold) {
        fprintf(out, "%-8s", "copies");
    }
    fprintf(out, "\n");
}

/* The interquartile range as a percentage of the median: how far apart
 * the middle half of the samples were, readable at a glance in a table. */
static double bmb_print_iqr_percent(const bmb_result_row_t *row)
{
    return (row->median_s > 0.0) ? (row->q3_s - row->q1_s) / row->median_s * 100.0 : 0.0; /* GCOVR_EXCL_BR_LINE: a call too short for the clock to resolve: not on Linux's nanosecond clock */
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
    fprintf(out, "%-16.9f%-10.1f", row->median_s, bmb_print_iqr_percent(row));
    if (bmb_print_adaptive(rs)) {
        fprintf(out, "%-10u", row->samples);
    }
    if (rs->cache_cold) {
        fprintf(out, "%-8u", row->copies);
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

    bmb_print_provenance(out, rs);
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
        fprintf(out, ",mean_s,stddev_s,max_s");
    }
    fprintf(out, ",calls_per_sample,median_s,q1_s,q3_s,samples");
    if (rs->cache_cold) {
        fprintf(out, ",copies");
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
            fprintf(out, ",%.9f,%.9f,%.9f", row->mean_s, row->stddev_s, row->max_s);
        }
        fprintf(out, ",%u,%.9f,%.9f,%.9f,%u", row->batch, row->median_s, row->q1_s, row->q3_s,
                row->samples);
        if (rs->cache_cold) {
            fprintf(out, ",%u", row->copies);
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

/* The machine block, and the date next to it. Every field that could not
 * be read is left out rather than written empty. */
static void bmb_print_json_machine(FILE *out, const bmb_machine_t *m)
{
    const char *sep = "\n";
    int i;

    if (m->date[0] != '\0') { /* GCOVR_EXCL_BR_LINE: time() and gmtime_r() of now do not fail */
        fprintf(out, "  \"date\": \"%s\",\n", m->date);
    }

    fprintf(out, "  \"machine\": {");
    if (m->cpu[0] != '\0') {
        fprintf(out, "%s    \"cpu\": ", sep);
        bmb_print_json_string(out, m->cpu);
        sep = ",\n";
    }
    if (m->logical_cpus > 0) { /* GCOVR_EXCL_BR_LINE: sysconf() always gives the CPU count on Linux */
        fprintf(out, "%s    \"logical_cpus\": %ld", sep, m->logical_cpus);
        sep = ",\n";
    }
    if (m->numa_nodes > 0) {
        fprintf(out, "%s    \"numa_nodes\": %d", sep, m->numa_nodes);
        sep = ",\n";
    }
    if (m->cache_count > 0) {
        fprintf(out, "%s    \"caches\": [", sep);
        for (i = 0; i < m->cache_count; i++) {
            fprintf(out, "%s\n      {\"level\": %d, \"type\": \"%s\", \"size_bytes\": %llu}",
                    (i == 0) ? "" : ",", m->caches[i].level, m->caches[i].type,
                    m->caches[i].size_bytes);
        }
        fprintf(out, "\n    ]");
        sep = ",\n";
    }
    if (m->governor[0] != '\0') {
        fprintf(out, "%s    \"governor\": ", sep);
        bmb_print_json_string(out, m->governor);
        sep = ",\n";
    }
    if (m->turbo >= 0) {
        fprintf(out, "%s    \"turbo\": %s", sep, m->turbo ? "true" : "false");
        sep = ",\n";
    }
    if (m->os[0] != '\0') { /* GCOVR_EXCL_BR_LINE: uname() cannot fail with a valid buffer */
        fprintf(out, "%s    \"os\": ", sep);
        bmb_print_json_string(out, m->os);
        sep = ",\n";
    }
    fprintf(out, "%s},\n", (sep[0] == ',') ? "\n  " : ""); /* GCOVR_EXCL_BR_LINE: the logical CPUs and the OS are always there, so never an empty object */
}

/* The run block: how the measurement was made (#161), the same as the
 * text's run lines. */
static void bmb_print_json_run(FILE *out, const bmb_result_set_t *rs, const bmb_machine_t *m)
{
    const char *sep = "";
    size_t i;
    int a;

    fprintf(out, "  \"run\": {\n");
    fprintf(out, "    \"command\": [");
    for (a = 0; a < rs->argc; a++) {
        fprintf(out, "%s", a == 0 ? "" : ", ");
        bmb_print_json_string(out, a == 0 ? bmb_print_program(rs->argv[0]) : rs->argv[a]);
    }
    fprintf(out, "],\n");
    fprintf(out, "    \"warmup\": %u,\n", rs->warmup);
    fprintf(out, "    \"warmup_time\": %g,\n", rs->warmup_time);
    fprintf(out, "    \"iterations\": %u,\n", rs->iterations);
    fprintf(out, "    \"min_time\": %g,\n", rs->min_time);
    fprintf(out, "    \"max_spread\": %g,\n", rs->max_spread);
    fprintf(out, "    \"max_time\": %g,\n", rs->max_time);
    if (rs->batch != 0) {
        fprintf(out, "    \"batch\": %u,\n", rs->batch);
    }
    fprintf(out, "    \"cache\": \"%s\",\n", rs->cache_cold ? "cold" : "hot");
    fprintf(out, "    \"align\": %u,\n", rs->align);
    if (rs->layout != NULL) {
        fprintf(out, "    \"ld_pad\": %u,\n", rs->ld_pad);
    }
    fprintf(out, "    \"compiler\": ");
    bmb_print_json_string(out, bmb_build_compiler());
    fprintf(out, ",\n    \"cflags\": ");
    bmb_print_json_string(out, bmb_build_cflags());
    fprintf(out, ",\n");
    if (m->cpus_allowed[0] != '\0') {
        fprintf(out, "    \"cpus_allowed\": \"%s\",\n", m->cpus_allowed);
    }
    fprintf(out, "    \"environment\": {");
    for (i = 0; i < BMB_RUN_VARIABLES; i++) {
        const char *value = getenv(bmb_run_variables[i]);

        if (value != NULL) {
            fprintf(out, "%s\"%s\": ", sep, bmb_run_variables[i]);
            bmb_print_json_string(out, value);
            sep = ", ";
        }
    }
    fprintf(out, "}\n  },\n");
}

void bmb_print_json(FILE *out, const bmb_result_set_t *rs)
{
    const char *blas = bmb_build_blas_version();
    size_t i;

    fprintf(out, "{\n");
    fprintf(out, "  \"version\": \"%s\",\n", bmb_build_version());
    fprintf(out, "  \"backend\": ");
    bmb_print_json_string(out, bmb_build_backend());
    fprintf(out, ",\n");
    if (blas != NULL && blas[0] != '\0') { /* GCOVR_EXCL_BR_LINE: no library this builds against reports an empty version */
        fprintf(out, "  \"blas\": ");
        bmb_print_json_string(out, blas);
        fprintf(out, ",\n");
    }
#if defined(BMB_BUILD_HAS_KERNELS)
    fprintf(out, "  \"kernels\": ");
    bmb_print_json_string(out, bmb_build_kernels());
    fprintf(out, ",\n");
#endif
    if (rs->label != NULL) {
        fprintf(out, "  \"label\": ");
        bmb_print_json_string(out, rs->label);
        fprintf(out, ",\n");
    }
    if (rs->verified) {
        fprintf(out, "  \"verified\": true,\n");
    }
    if (rs->layout != NULL) {
        fprintf(out, "  \"layout\": \"%s\",\n", rs->layout);
    }
    bmb_print_json_machine(out, bmb_machine());
    bmb_print_json_run(out, rs, bmb_machine());
    fprintf(out, "  \"routine\": \"%s\",\n", rs->routine_name);

    /* What dim1 and dim2 *are* for this routine ("Matrix dim1 (M=K)"). The
     * text output has always carried them in its header; without them here
     * a reader of the JSON -- bmb_report first of all -- could only title
     * its axes by keeping its own copy of each routine's conventions, which
     * would drift from the C files that define them. dim2_label is omitted
     * for routines with a single dimension, like dim2 itself. */
    fprintf(out, "  \"dim1_label\": ");
    bmb_print_json_string(out, rs->dim1_label);
    fprintf(out, ",\n");
    if (rs->dim2_label != NULL) {
        fprintf(out, "  \"dim2_label\": ");
        bmb_print_json_string(out, rs->dim2_label);
        fprintf(out, ",\n");
    }

    fprintf(out, "  \"results\": [\n");
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
                         "\n      \"max_s\": %.9f",
                    row->mean_s, row->stddev_s, row->max_s);
        }
        fprintf(out, ",\n      \"calls_per_sample\": %u,\n      \"median_s\": %.9f,"
                     "\n      \"q1_s\": %.9f,\n      \"q3_s\": %.9f,\n      \"samples\": %u",
                row->batch, row->median_s, row->q1_s, row->q3_s, row->samples);
        if (rs->cache_cold) {
            fprintf(out, ",\n      \"copies\": %u", row->copies);
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
