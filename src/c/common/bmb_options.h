#ifndef BMB_OPTIONS_H
#define BMB_OPTIONS_H

#include <stddef.h>

typedef enum {
    BMB_FORMAT_CSV,
    BMB_FORMAT_JSON
} bmb_output_format_t;

typedef struct {
    size_t min;
    size_t max;
} bmb_range_t;

typedef struct {
    unsigned int warmup;      /* -x, --warmup */
    unsigned int iterations;  /* -i, --iterations */

    bmb_range_t vector_size;  /* -v, --vector-size    (level 1) */
    int vector_size_set;      /* whether -v/--vector-size was given */

    bmb_range_t matrix_dim1;  /* -m, --matrix-dim1    (level 2 & 3) */
    int matrix_dim1_set;      /* whether -m/--matrix-dim1 was given */
    bmb_range_t matrix_dim2;  /* -M, --matrix-dim2    (level 2 & 3) */
    int matrix_dim2_set;      /* whether -M/--matrix-dim2 was given */

    bmb_range_t thread_count; /* -t, --thread-count */
    int thread_count_set;     /* whether -t/--thread-count was given */

    int statistics;           /* -s, --statistics */

    char *output_file;        /* -o, --output */
    bmb_output_format_t output_format;
    int output_format_set;    /* whether -f/--output-format was given */
} bmb_options_t;

typedef enum {
    BMB_OPTIONS_OK = 0,
    BMB_OPTIONS_HELP,
    BMB_OPTIONS_ERROR
} bmb_options_status_t;

/* Parses argv into opts, filling defaults for anything not given on the
 * command line. Returns BMB_OPTIONS_HELP if -h/--help was requested (the
 * usage message has already been printed) and BMB_OPTIONS_ERROR on a
 * malformed command line (an error message has already been printed);
 * in both cases the caller should stop and return early. */
bmb_options_status_t bmb_options_parse(int argc, char *argv[], bmb_options_t *opts);

void bmb_options_free(bmb_options_t *opts);

void bmb_options_print_help(const char *prog_name);

#endif /* BMB_OPTIONS_H */
