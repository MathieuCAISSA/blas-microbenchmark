#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_options.h"
#include "bmb_log.h"

#define BMB_DEFAULT_WARMUP       1u
#define BMB_DEFAULT_ITERATIONS   10u
#define BMB_DEFAULT_SIZE         4096u
#define BMB_DEFAULT_THREADS      1u

static const struct option bmb_long_options[] = {
    {"warmup",         required_argument, NULL, 'x'},
    {"iterations",     required_argument, NULL, 'i'},
    {"vector-size",    required_argument, NULL, 'v'},
    {"matrix-dim1",    required_argument, NULL, 'm'},
    {"matrix-dim2",    required_argument, NULL, 'M'},
    {"thread-count",   required_argument, NULL, 't'},
    {"statistics",     no_argument,       NULL, 's'},
    {"output",         required_argument, NULL, 'o'},
    {"output-format",  required_argument, NULL, 'f'},
    {"help",           no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
};

static const char *bmb_short_options = "x:i:v:m:M:t:so:f:h";

/* Sizes and thread counts both end up as `int` arguments to BLAS, which is
 * a 32-bit integer in every LP64 build. Anything larger has to be refused
 * up front: the cast would silently truncate, and a dimension landing on 0
 * would time an empty call and report it as a result. */
#define BMB_MAX_VALUE ((unsigned long long) INT_MAX)

#define BMB_PARSE_OK        0
#define BMB_PARSE_MALFORMED (-1)
#define BMB_PARSE_RANGE     (-2)

/* Parses a bare unsigned decimal integer with none of strtoull's leniency:
 * no sign, no leading blanks, no trailing characters. In particular "-5"
 * must not be accepted -- strtoull happily wraps it around to a huge
 * value. */
static int bmb_parse_uint(const char *str, unsigned long long limit, unsigned long long *out)
{
    char *endptr;
    unsigned long long value;

    if (str == NULL || str[0] < '0' || str[0] > '9') {
        return BMB_PARSE_MALFORMED;
    }

    errno = 0;
    value = strtoull(str, &endptr, 10);
    if (*endptr != '\0') {
        return BMB_PARSE_MALFORMED;
    }
    if (errno != 0 || value > limit) {
        return BMB_PARSE_RANGE;
    }

    *out = value;
    return BMB_PARSE_OK;
}

/* Parses a "[min:]max" range of positive integers. */
static int bmb_parse_range(const char *str, bmb_range_t *out)
{
    char buf[64];
    char *colon;
    unsigned long long min_val;
    unsigned long long max_val;
    int status;

    if (str == NULL || str[0] == '\0' || strlen(str) >= sizeof(buf)) {
        return BMB_PARSE_MALFORMED;
    }
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    colon = strchr(buf, ':');
    if (colon != NULL) {
        *colon = '\0';

        status = bmb_parse_uint(buf, BMB_MAX_VALUE, &min_val);
        if (status != BMB_PARSE_OK) {
            return status;
        }

        status = bmb_parse_uint(colon + 1, BMB_MAX_VALUE, &max_val);
        if (status != BMB_PARSE_OK) {
            return status;
        }
    } else {
        status = bmb_parse_uint(buf, BMB_MAX_VALUE, &max_val);
        if (status != BMB_PARSE_OK) {
            return status;
        }
        min_val = max_val;
    }

    if (min_val == 0 || max_val == 0 || min_val > max_val) {
        return BMB_PARSE_MALFORMED;
    }

    out->min = (size_t) min_val;
    out->max = (size_t) max_val;
    return BMB_PARSE_OK;
}

/* Parses one range option, reporting what was wrong with it. */
static int bmb_option_range(const char *str, const char *option, bmb_range_t *out)
{
    char msg[256];

    switch (bmb_parse_range(str, out)) {
    case BMB_PARSE_OK:
        return 0;
    case BMB_PARSE_RANGE:
        snprintf(msg, sizeof(msg),
                 "Value out of range for %s: each bound must be between 1 and %d.",
                 option, INT_MAX);
        break;
    default:
        snprintf(msg, sizeof(msg),
                 "Invalid value for %s (expected [min:]max, positive integers).", option);
        break;
    }

    bmb_log_error(msg);
    return -1;
}

/* Parses one plain count option (--warmup, --iterations). */
static int bmb_option_count(const char *str, const char *option, unsigned int min_allowed,
                            unsigned int *out)
{
    unsigned long long value;
    char msg[256];

    if (bmb_parse_uint(str, (unsigned long long) UINT_MAX, &value) != BMB_PARSE_OK
        || value < (unsigned long long) min_allowed) {
        snprintf(msg, sizeof(msg), "Invalid value for %s (expected an integer between %u and %u).",
                 option, min_allowed, UINT_MAX);
        bmb_log_error(msg);
        return -1;
    }

    *out = (unsigned int) value;
    return 0;
}

static void bmb_options_set_defaults(bmb_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));

    opts->warmup = BMB_DEFAULT_WARMUP;
    opts->iterations = BMB_DEFAULT_ITERATIONS;

    opts->vector_size.min = BMB_DEFAULT_SIZE;
    opts->vector_size.max = BMB_DEFAULT_SIZE;
    opts->vector_size_set = 0;

    opts->matrix_dim1.min = BMB_DEFAULT_SIZE;
    opts->matrix_dim1.max = BMB_DEFAULT_SIZE;
    opts->matrix_dim1_set = 0;
    opts->matrix_dim2.min = BMB_DEFAULT_SIZE;
    opts->matrix_dim2.max = BMB_DEFAULT_SIZE;
    opts->matrix_dim2_set = 0;

    opts->thread_count.min = BMB_DEFAULT_THREADS;
    opts->thread_count.max = BMB_DEFAULT_THREADS;
    opts->thread_count_set = 0;

    opts->statistics = 0;

    opts->output_file = NULL;
    opts->output_format = BMB_FORMAT_CSV;
    opts->output_format_set = 0;
}

static int bmb_infer_output_format(const char *filename)
{
    size_t len = strlen(filename);

    if (len >= 5 && strcmp(filename + len - 5, ".json") == 0) {
        return 1; /* json */
    }
    return 0; /* csv */
}

bmb_options_status_t bmb_options_parse(int argc, char *argv[], bmb_options_t *opts)
{
    int c;
    char errbuf[256];

    bmb_options_set_defaults(opts);

    opterr = 0;
    optind = 1;

    while ((c = getopt_long(argc, argv, bmb_short_options, bmb_long_options, NULL)) != -1) {
        switch (c) {
        case 'x':
            if (bmb_option_count(optarg, "--warmup", 0, &opts->warmup) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'i':
            if (bmb_option_count(optarg, "--iterations", 1, &opts->iterations) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'v':
            if (bmb_option_range(optarg, "--vector-size", &opts->vector_size) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            opts->vector_size_set = 1;
            break;

        case 'm':
            if (bmb_option_range(optarg, "--matrix-dim1", &opts->matrix_dim1) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            opts->matrix_dim1_set = 1;
            break;

        case 'M':
            if (bmb_option_range(optarg, "--matrix-dim2", &opts->matrix_dim2) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            opts->matrix_dim2_set = 1;
            break;

        case 't':
            if (bmb_option_range(optarg, "--thread-count", &opts->thread_count) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            opts->thread_count_set = 1;
            break;

        case 's':
            opts->statistics = 1;
            break;

        case 'o':
            free(opts->output_file);
            opts->output_file = strdup(optarg);
            break;

        case 'f':
            if (strcmp(optarg, "csv") == 0) {
                opts->output_format = BMB_FORMAT_CSV;
            } else if (strcmp(optarg, "json") == 0) {
                opts->output_format = BMB_FORMAT_JSON;
            } else {
                bmb_log_error("Invalid value for --output-format (expected csv or json).");
                return BMB_OPTIONS_ERROR;
            }
            opts->output_format_set = 1;
            break;

        case 'h':
            bmb_options_print_help(argv[0]);
            return BMB_OPTIONS_HELP;

        case '?':
        default:
            snprintf(errbuf, sizeof(errbuf), "Unknown or malformed option: %s",
                     optind > 0 && optind <= argc ? argv[optind - 1] : "?");
            bmb_log_error(errbuf);
            bmb_options_print_help(argv[0]);
            return BMB_OPTIONS_ERROR;
        }
    }

    if (!opts->matrix_dim2_set) {
        opts->matrix_dim2 = opts->matrix_dim1;
    }

    if (opts->output_file != NULL && !opts->output_format_set) {
        opts->output_format = bmb_infer_output_format(opts->output_file) ? BMB_FORMAT_JSON : BMB_FORMAT_CSV;
    }

    return BMB_OPTIONS_OK;
}

void bmb_options_free(bmb_options_t *opts)
{
    free(opts->output_file);
    opts->output_file = NULL;
}

void bmb_options_print_help(const char *prog_name)
{
    fprintf(stdout, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stdout,
        "  -x, --warmup <n>              iterations ignored before timing (default: %u)\n"
        "  -i, --iterations <n>          iterations measured (default: %u)\n"
        "  -v, --vector-size <[min:]max> vector size range for level 1 routines (default: %u)\n"
        "  -m, --matrix-dim1 <[min:]max> matrix first-dimension range for level 2 & 3 (default: %u)\n"
        "  -M, --matrix-dim2 <[min:]max> matrix second-dimension range for level 2 & 3\n"
        "                                 (default: same as --matrix-dim1, i.e. square matrices)\n"
        "  -t, --thread-count <[min:]max> number of BLAS threads (default: %u)\n"
        "  -s, --statistics              add stddev/min/max columns (default: off)\n"
        "  -o, --output <filename>       also save results to filename\n"
        "  -f, --output-format <fmt>     csv or json (default: csv, or inferred from -o's extension)\n"
        "  -h, --help                    show this help\n",
        BMB_DEFAULT_WARMUP, BMB_DEFAULT_ITERATIONS, BMB_DEFAULT_SIZE, BMB_DEFAULT_SIZE, BMB_DEFAULT_THREADS);
}
