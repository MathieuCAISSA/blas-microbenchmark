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

/* The default size of each level (#165): out of the last-level cache at
 * levels 1 and 2, where bandwidth is what is measured; under a second a
 * dgemm call at level 3 on one core; none a power of two, which would put
 * every row of a matrix on the same cache sets (#163). */
#define BMB_DEFAULT_VECTOR_SIZE  10000000u
#define BMB_DEFAULT_LEVEL2_SIZE  5000u
#define BMB_DEFAULT_LEVEL3_SIZE  2000u

/* Reduces range to the single point value. */
void bmb_range_set_single(bmb_range_t *range, size_t value);

/* Writes a short human-readable form of range into buf ("4", or
 * "1, 2, 4, 8", elided with an ellipsis when there are many points). What
 * does not fit in size bytes is left out whole, never cut. */
void bmb_range_describe(const bmb_range_t *range, char *buf, size_t size);

typedef struct {
    unsigned int warmup;      /* -x, --warmup */
    unsigned int iterations;  /* -i, --iterations */
    unsigned int batch;       /* -b, --batch; 0 = choose automatically */

    bmb_range_t vector_size;  /* -v, --vector-size    (level 1) */
    int vector_size_set;      /* whether -v/--vector-size was given */

    bmb_range_t matrix_dim1;  /* -m, --matrix-dim1    (level 2 & 3) */
    int matrix_dim1_set;      /* whether -m/--matrix-dim1 was given */
    bmb_range_t matrix_dim2;  /* -M, --matrix-dim2    (level 2 & 3) */
    int matrix_dim2_set;      /* whether -M was given; without it dim2
                               * follows dim1 point by point (square
                               * matrices) and matrix_dim2 is unused */
    bmb_range_t matrix_dim3;  /* -K, --matrix-dim3    (GEMM's K, #215) */
    int matrix_dim3_set;      /* whether -K was given; without it K is
                               * dim1 (M) and matrix_dim3 is unused */

    bmb_range_t thread_count; /* -t, --thread-count */
    int thread_count_set;     /* whether -t/--thread-count was given */

    int statistics;           /* -s, --statistics */
    int verify;               /* -c, --verify */
    int column_major;         /* -L, --layout col: matrices stored and
                               * passed column-major, as Fortran does */
    int layout_set;           /* whether -L/--layout was given */
    int cache_cold;           /* -k, --cache cold: each timed call finds its
                               * operands out of the caches (#160) */
    unsigned int align;       /* -a, --align: the bytes every operand is
                               * aligned to (#163) */
    unsigned int ld_pad;      /* -p, --ld-pad: what is added to a matrix's
                               * leading dimension, in elements (#163) */
    int ld_pad_set;           /* whether -p/--ld-pad was given */
    /* Time-based warm-up and sampling (#164), in seconds; 0 is off. */
    double warmup_time;       /* -w, --warmup-time: calls repeated for this
                               * long before the first point of each thread
                               * count, on top of -x */
    double min_time;          /* -T, --min-time: samples beyond -i until
                               * their total reaches it */
    double max_spread;        /* -e, --max-spread: then samples until their
                               * interquartile range is at most this, in
                               * percent of their median */
    double max_time;          /* -E, --max-time: past it, sampling stops
                               * whatever -T and -e ask */

    char *label;              /* -l, --label; NULL when not given */

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
