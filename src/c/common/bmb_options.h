#ifndef BMB_OPTIONS_H
#define BMB_OPTIONS_H

#include <stddef.h>

typedef enum {
    BMB_FORMAT_CSV,
    BMB_FORMAT_JSON
} bmb_output_format_t;

/* A sweep is expanded into its explicit list of points when the command
 * line is parsed, so the benchmark loop is a plain iteration and the
 * endpoint cannot be missed by an off-by-one in a stride. The bound keeps
 * the struct small enough to hold by value and rules out a spec like
 * 1:1000000:1 asking for a million measurements. */
#define BMB_MAX_SWEEP_POINTS 256

typedef struct {
    size_t values[BMB_MAX_SWEEP_POINTS]; /* measured in this order */
    size_t count;
} bmb_range_t;

/* Reduces range to the single point value. */
void bmb_range_set_single(bmb_range_t *range, size_t value);

/* Writes a short human-readable form of range into buf ("4", or
 * "1, 2, 4, 8", elided with an ellipsis when there are many points). */
void bmb_range_describe(const bmb_range_t *range, char *buf, size_t size);

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
    BMB_OPTIONS_VERSION,
    BMB_OPTIONS_ERROR
} bmb_options_status_t;

/* Parses argv into opts, filling defaults for anything not given on the
 * command line. Returns BMB_OPTIONS_HELP for -h/--help, BMB_OPTIONS_VERSION
 * for -V/--version (the message has already been printed in both cases),
 * and BMB_OPTIONS_ERROR on a malformed command line (an error message has
 * already been printed). In all three the caller should stop and return
 * early. */
bmb_options_status_t bmb_options_parse(int argc, char *argv[], bmb_options_t *opts);

void bmb_options_free(bmb_options_t *opts);

void bmb_options_print_help(const char *prog_name);

void bmb_options_print_version(void);

#endif /* BMB_OPTIONS_H */
