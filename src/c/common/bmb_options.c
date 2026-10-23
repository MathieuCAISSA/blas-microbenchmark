#define _POSIX_C_SOURCE 200809L

#include <config.h>

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_options.h"
#include "bmb_build.h"
#include "bmb_machine.h"
#include "bmb_log.h"

#define BMB_DEFAULT_WARMUP       1u
#define BMB_DEFAULT_ITERATIONS   10u
#define BMB_DEFAULT_THREADS      1u
#define BMB_DEFAULT_ALIGN        64u
/* A cache line on most of today's CPUs, and the most any vector
 * instruction asks for (AVX-512); up to a 2 MiB huge page. */
#define BMB_MIN_ALIGN            8u
#define BMB_MAX_ALIGN            2097152u
/* Enough to step off any power-of-two stride a cache or a TLB cares
 * about. Small enough that a size the parser accepts, plus it, still fits
 * the int BLAS takes: 1518500249 + 65536 < INT_MAX. */
#define BMB_MAX_LD_PAD           65536u
/* Long enough for a CPU to leave its idle clock under the schedutil or
 * powersave governors, which take tens of milliseconds; short enough to
 * pay once for each thread count of a run (#164). */
#define BMB_DEFAULT_WARMUP_TIME  0.2
#define BMB_DEFAULT_MAX_TIME     10.0
#define BMB_MAX_SECONDS          3600.0

static const struct option bmb_long_options[] = {
    {"warmup",         required_argument, NULL, 'x'},
    {"iterations",     required_argument, NULL, 'i'},
    {"batch",          required_argument, NULL, 'b'},
    {"vector-size",    required_argument, NULL, 'v'},
    {"matrix-dim1",    required_argument, NULL, 'm'},
    {"matrix-dim2",    required_argument, NULL, 'M'},
    {"thread-count",   required_argument, NULL, 't'},
    {"statistics",     no_argument,       NULL, 's'},
    {"verify",         no_argument,       NULL, 'c'},
    {"no-verify",      no_argument,       NULL, 'C'},
    {"layout",         required_argument, NULL, 'L'},
    {"cache",          required_argument, NULL, 'k'},
    {"align",          required_argument, NULL, 'a'},
    {"ld-pad",         required_argument, NULL, 'p'},
    {"warmup-time",    required_argument, NULL, 'w'},
    {"min-time",       required_argument, NULL, 'T'},
    {"max-spread",     required_argument, NULL, 'e'},
    {"max-time",       required_argument, NULL, 'E'},
    {"label",          required_argument, NULL, 'l'},
    {"output",         required_argument, NULL, 'o'},
    {"output-format",  required_argument, NULL, 'f'},
    {"help",           no_argument,       NULL, 'h'},
    {"version",        no_argument,       NULL, 'V'},
    {NULL, 0, NULL, 0}
};

static const char *bmb_short_options = "x:i:b:v:m:M:t:scCL:k:a:p:w:T:e:E:l:o:f:hV";

/* The largest value any size option may take.
 *
 * Two separate ceilings, whichever is lower:
 *
 * - INT_MAX, because sizes end up as `int` arguments to BLAS (a 32-bit
 *   integer in every LP64 build). A larger value would truncate on the
 *   cast, and a dimension landing on 0 would time an empty call and report
 *   the result as if it meant something.
 * - the largest d for which a d x d matrix of doubles can be *sized*
 *   without wrapping size_t. Several routines allocate dim1 x dim1 (dgemm
 *   ties K to M; dsymm/dtrmm/dtrsm hold an M x M triangle; every square
 *   level 2 routine holds an N x N matrix), and `n * n * sizeof(double)`
 *   wrapping does not fail: it comes back down to a small number, malloc
 *   succeeds, and the setup loop then writes n^2 doubles into it. At
 *   n = 1518500250 that is a 291 MB buffer and 2.3e18 elements.
 *
 * On 64-bit the second ceiling is 1518500249, which only rules out square
 * matrices that would need 18 exabytes -- nothing reachable is lost. On a
 * 32-bit build it correctly drops to about 23170, where a d x d matrix
 * already fills the address space. */
static unsigned long long bmb_max_value(void)
{
    const size_t elements = SIZE_MAX / sizeof(double);
    size_t lo = 1;
    size_t hi = (size_t) INT_MAX;

    while (lo < hi) {
        size_t mid = lo + (hi - lo + 1) / 2;

        if (mid <= elements / mid) {
            lo = mid;
        } else {
            hi = mid - 1;
        }
    }

    return (unsigned long long) lo;
}

#define BMB_PARSE_OK        0
#define BMB_PARSE_MALFORMED (-1)
#define BMB_PARSE_RANGE     (-2)
#define BMB_PARSE_TOO_MANY  (-3)

/* Parses a bare unsigned decimal integer with none of strtoull's leniency:
 * no sign, no leading blanks, no trailing characters. In particular "-5"
 * must not be accepted -- strtoull happily wraps it around to a huge
 * value. */
static int bmb_parse_uint(const char *str, unsigned long long limit, unsigned long long *out)
{
    char *endptr;
    unsigned long long value;

    if (str[0] < '0' || str[0] > '9') {
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

void bmb_range_set_single(bmb_range_t *range, size_t value)
{
    range->values[0] = value;
    range->count = 1;
}

void bmb_range_describe(const bmb_range_t *range, char *buf, size_t size)
{
    /* Enough points to recognise the sweep, not so many that a warning
     * turns into a wall of numbers. */
    const size_t shown = 4;
    size_t used = 0;
    size_t i;

    if (size == 0) {
        return;
    }
    buf[0] = '\0';

    for (i = 0; i < range->count && i < shown; i++) {
        int n = snprintf(buf + used, size - used, "%s%zu",
                         (i == 0) ? "" : ", ", range->values[i]);

        /* A point that does not fit is left out whole: cut, 1000 would
         * read as 10, a size nobody asked for. */
        if (n < 0 || (size_t) n >= size - used) { /* GCOVR_EXCL_BR_LINE: snprintf does not fail on these formats, n < 0 never holds */
            buf[used] = '\0';
            return;
        }
        used += (size_t) n;
    }

    if (range->count > shown) {
        int n = snprintf(buf + used, size - used, ", ... (%zu points)", range->count);

        if (n < 0 || (size_t) n >= size - used) { /* GCOVR_EXCL_BR_LINE: snprintf does not fail on these formats, n < 0 never holds */
            buf[used] = '\0';
        }
    }
}

static unsigned long long bmb_range_max(const bmb_range_t *range)
{
    size_t i, max = 0;

    for (i = 0; i < range->count; i++) {
        max = (range->values[i] > max) ? range->values[i] : max;
    }
    return (unsigned long long) max;
}

static int bmb_range_append(bmb_range_t *out, size_t value)
{
    if (out->count >= BMB_MAX_SWEEP_POINTS) {
        return BMB_PARSE_TOO_MANY;
    }
    out->values[out->count++] = value;
    return BMB_PARSE_OK;
}

/* Expands min..max into out, moving by `step` when it is non-zero and
 * doubling otherwise. max is always the last point: a stride that would
 * overshoot it is clamped to it instead, so the endpoint a user asked for
 * is the endpoint that gets measured. */
static int bmb_range_expand(bmb_range_t *out, size_t min, size_t max, size_t step)
{
    size_t v = min;

    for (;;) {
        int status = bmb_range_append(out, v);

        if (status != BMB_PARSE_OK) {
            return status;
        }
        if (v >= max) {
            return BMB_PARSE_OK;
        }

        if (step != 0) {
            v = (max - v < step) ? max : v + step;
        } else {
            v = (v > max / 2) ? max : v * 2;
        }
    }
}

/* Parses one sweep specification:
 *
 *   <max>                a single point
 *   <min>:<max>          doubling from min to max
 *   <min>:<max>:<step>   linear, in steps of step
 *   <v1>,<v2>,...        exactly these points, in this order
 */
static int bmb_parse_range(const char *str, bmb_range_t *out)
{
    char buf[512];
    char *field[3];
    unsigned long long bound[3] = {0, 0, 0};
    char *p;
    size_t nfields = 0;
    size_t i;
    int status;

    if (str[0] == '\0' || strlen(str) >= sizeof(buf)) {
        return BMB_PARSE_MALFORMED;
    }
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    out->count = 0;

    if (strchr(buf, ',') != NULL) {
        if (strchr(buf, ':') != NULL) {
            return BMB_PARSE_MALFORMED; /* a list has no endpoints to stride between */
        }

        for (p = buf; p != NULL; ) {
            char *comma = strchr(p, ',');
            unsigned long long value;

            if (comma != NULL) {
                *comma = '\0';
            }

            status = bmb_parse_uint(p, bmb_max_value(), &value);
            if (status != BMB_PARSE_OK) {
                return status;
            }
            if (value == 0) {
                return BMB_PARSE_MALFORMED;
            }

            status = bmb_range_append(out, (size_t) value);
            if (status != BMB_PARSE_OK) { /* GCOVR_EXCL_BR_LINE: 257 points take 513 characters, past the 511 a sweep may have */
                return status; /* GCOVR_EXCL_LINE: 257 points take 513 characters, past the 511 a sweep may have */
            }

            p = (comma != NULL) ? comma + 1 : NULL;
        }

        return BMB_PARSE_OK;
    }

    field[nfields++] = buf;
    for (p = buf; *p != '\0'; p++) {
        if (*p != ':') {
            continue;
        }
        if (nfields == 3) {
            return BMB_PARSE_MALFORMED;
        }
        *p = '\0';
        field[nfields++] = p + 1;
    }

    for (i = 0; i < nfields; i++) {
        status = bmb_parse_uint(field[i], bmb_max_value(), &bound[i]);
        if (status != BMB_PARSE_OK) {
            return status;
        }
        if (bound[i] == 0) {
            return BMB_PARSE_MALFORMED;
        }
    }

    if (nfields == 1) {
        bmb_range_set_single(out, (size_t) bound[0]);
        return BMB_PARSE_OK;
    }

    if (bound[0] > bound[1]) {
        return BMB_PARSE_MALFORMED;
    }

    return bmb_range_expand(out, (size_t) bound[0], (size_t) bound[1],
                            (nfields == 3) ? (size_t) bound[2] : 0);
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
                 "Value out of range for %s: every value must be between 1 and %llu.",
                 option, bmb_max_value());
        break;
    case BMB_PARSE_TOO_MANY:
        snprintf(msg, sizeof(msg),
                 "Too many points for %s: at most %d (use a larger step, or list the sizes).",
                 option, BMB_MAX_SWEEP_POINTS);
        break;
    default:
        snprintf(msg, sizeof(msg),
                 "Invalid value for %s (expected max, min:max, min:max:step, or a v1,v2,... list).",
                 option);
        break;
    }

    bmb_log_error(msg);
    return -1;
}

/* Parses a duration or a percentage: digits, then optionally a point and
 * more digits ("0.2", "10"), between 0 and `max`. Nothing strtod would
 * take besides: no sign, no exponent, no "inf" or "nan". */
static int bmb_option_decimal(const char *str, const char *option, double max, double *out)
{
    const char *p = str;
    char msg[256];
    double value;

    while (*p >= '0' && *p <= '9') {
        p++;
    }
    if (p > str && *p == '.') {
        const char *frac = ++p;

        while (*p >= '0' && *p <= '9') {
            p++;
        }
        if (p == frac) {
            p = str;
        }
    }
    value = (p > str && *p == '\0') ? strtod(str, NULL) : -1.0;
    if (value < 0.0 || value > max) {
        snprintf(msg, sizeof(msg), "Invalid value for %s (expected a number from 0 to %g, e.g. 0.5).",
                 option, max);
        bmb_log_error(msg);
        return -1;
    }
    *out = value;
    return 0;
}

/* Parses one plain count option (--warmup, --iterations). */
static int bmb_option_count(const char *str, const char *option, unsigned int min_allowed,
                            unsigned int *out)
{
    unsigned long long value;
    char msg[256];

    if (bmb_parse_uint(str, (unsigned long long) UINT_MAX, &value) != BMB_PARSE_OK
        || value < (unsigned long long) min_allowed) {
        /* UINT_MAX is an unsigned int; cppcheck's own limits.h makes it a
         * long. GCC's -Wformat checks the real one. */
        /* cppcheck-suppress invalidPrintfArgType_uint */
        snprintf(msg, sizeof(msg), "Invalid value for %s (expected an integer between %u and %u).",
                 option, min_allowed, UINT_MAX);
        bmb_log_error(msg);
        return -1;
    }

    *out = (unsigned int) value;
    return 0;
}

/* The free-text label for what no automatic field can see: turbo on or
 * off, before and after a BIOS update, two NUMA bindings, two identical
 * nodes (#1, decision 4). It is printed on a comment line of the text and
 * CSV output, so a newline in it would turn the rest into data rows, and a
 * control character has no business in a legend. */
#define BMB_MAX_LABEL 200

static int bmb_option_label(const char *str, char **out)
{
    char msg[256];
    size_t len = strlen(str);
    size_t i;
    int printable = (len > 0 && len <= BMB_MAX_LABEL);

    for (i = 0; printable && i < len; i++) {
        unsigned char c = (unsigned char) str[i];

        if (c < 0x20 || c == 0x7f) {
            printable = 0;
        }
    }

    if (!printable) {
        snprintf(msg, sizeof(msg),
                 "Invalid value for --label (expected 1 to %d characters, with no newline "
                 "or other control character).", BMB_MAX_LABEL);
        bmb_log_error(msg);
        return -1;
    }

    free(*out);
    *out = strdup(str);
    return 0;
}

static void bmb_options_set_defaults(bmb_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));

    opts->warmup = BMB_DEFAULT_WARMUP;
    opts->iterations = BMB_DEFAULT_ITERATIONS;
    opts->batch = 0;

    bmb_range_set_single(&opts->vector_size, BMB_DEFAULT_VECTOR_SIZE);
    opts->vector_size_set = 0;

    bmb_range_set_single(&opts->matrix_dim1, BMB_DEFAULT_LEVEL2_SIZE);
    opts->matrix_dim1_set = 0;
    bmb_range_set_single(&opts->matrix_dim2, BMB_DEFAULT_LEVEL2_SIZE);
    opts->matrix_dim2_set = 0;

    bmb_range_set_single(&opts->thread_count, BMB_DEFAULT_THREADS);
    opts->thread_count_set = 0;

    opts->statistics = 0;
    /* On by default, as STREAM and HPL check theirs: a number from a
     * library that computes the wrong thing is worth less than none, and
     * the check costs next to nothing (see bmb_verify.h). */
    opts->verify = 1;
    opts->column_major = 0;
    opts->layout_set = 0;
    opts->cache_cold = 0;
    opts->align = BMB_DEFAULT_ALIGN;
    opts->ld_pad = 0;
    opts->ld_pad_set = 0;
    opts->warmup_time = BMB_DEFAULT_WARMUP_TIME;
    opts->min_time = 0.0;
    opts->max_spread = 0.0;
    opts->max_time = BMB_DEFAULT_MAX_TIME;

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

        case 'b':
            if (bmb_option_count(optarg, "--batch", 0, &opts->batch) != 0) {
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

        case 'c':
            opts->verify = 1;
            break;

        case 'C':
            opts->verify = 0;
            break;

        case 'L':
            if (strcmp(optarg, "row") == 0) {
                opts->column_major = 0;
            } else if (strcmp(optarg, "col") == 0) {
                opts->column_major = 1;
            } else {
                bmb_log_error("Invalid value for --layout (expected row or col).");
                return BMB_OPTIONS_ERROR;
            }
            opts->layout_set = 1;
            break;

        case 'k':
            if (strcmp(optarg, "hot") == 0) {
                opts->cache_cold = 0;
            } else if (strcmp(optarg, "cold") == 0) {
                opts->cache_cold = 1;
            } else {
                bmb_log_error("Invalid value for --cache (expected hot or cold).");
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'a': {
            unsigned long long value;

            /* posix_memalign() takes a power of two, at least a pointer's
             * size. */
            if (bmb_parse_uint(optarg, BMB_MAX_ALIGN, &value) != BMB_PARSE_OK
                || value < BMB_MIN_ALIGN || (value & (value - 1)) != 0) {
                snprintf(errbuf, sizeof(errbuf),
                         "Invalid value for --align (expected a power of two from %u to %u).",
                         BMB_MIN_ALIGN, BMB_MAX_ALIGN);
                bmb_log_error(errbuf);
                return BMB_OPTIONS_ERROR;
            }
            opts->align = (unsigned int) value;
            break;
        }

        case 'p': {
            unsigned long long value;

            if (bmb_parse_uint(optarg, BMB_MAX_LD_PAD, &value) != BMB_PARSE_OK) {
                snprintf(errbuf, sizeof(errbuf),
                         "Invalid value for --ld-pad (expected an integer between 0 and %u).",
                         BMB_MAX_LD_PAD);
                bmb_log_error(errbuf);
                return BMB_OPTIONS_ERROR;
            }
            opts->ld_pad = (unsigned int) value;
            opts->ld_pad_set = 1;
            break;
        }

        case 'w':
            if (bmb_option_decimal(optarg, "--warmup-time", BMB_MAX_SECONDS, &opts->warmup_time) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'T':
            if (bmb_option_decimal(optarg, "--min-time", BMB_MAX_SECONDS, &opts->min_time) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'e':
            if (bmb_option_decimal(optarg, "--max-spread", 100.0, &opts->max_spread) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'E':
            if (bmb_option_decimal(optarg, "--max-time", BMB_MAX_SECONDS, &opts->max_time) != 0) {
                return BMB_OPTIONS_ERROR;
            }
            break;

        case 'l':
            if (bmb_option_label(optarg, &opts->label) != 0) {
                return BMB_OPTIONS_ERROR;
            }
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

        case 'V':
            bmb_options_print_version();
            return BMB_OPTIONS_VERSION;

        case '?':
        default:
            snprintf(errbuf, sizeof(errbuf), "Unknown or malformed option: %s", optind <= argc ? argv[optind - 1] : "?"); /* GCOVR_EXCL_BR_LINE: glibc's getopt, the coverage builds', never leaves optind past argc; the guard is for another libc's */
            bmb_log_error(errbuf);
            bmb_options_print_help(argv[0]);
            return BMB_OPTIONS_ERROR;
        }
    }

    if (opts->output_file != NULL && !opts->output_format_set) {
        opts->output_format = bmb_infer_output_format(opts->output_file) ? BMB_FORMAT_JSON : BMB_FORMAT_CSV;
    }

    /* Sampling stops at --max-time: a --min-time past it could not be met. */
    if (opts->min_time > opts->max_time) {
        snprintf(errbuf, sizeof(errbuf), "--min-time %g is past --max-time %g.", opts->min_time,
                 opts->max_time);
        bmb_log_error(errbuf);
        return BMB_OPTIONS_ERROR;
    }

    /* A matrix size plus --ld-pad within the ceiling (#163): a padded
     * matrix of elements of 8 bytes or less then cannot wrap size_t
     * either, so only the routines with larger ones check theirs. */
    if (bmb_range_max(&opts->matrix_dim1) + opts->ld_pad > bmb_max_value()
        || bmb_range_max(&opts->matrix_dim2) + opts->ld_pad > bmb_max_value()) {
        snprintf(errbuf, sizeof(errbuf),
                 "With --ld-pad %u, a matrix size is at most %llu.", opts->ld_pad,
                 bmb_max_value() - opts->ld_pad);
        bmb_log_error(errbuf);
        return BMB_OPTIONS_ERROR;
    }

    return BMB_OPTIONS_OK;
}

void bmb_options_free(bmb_options_t *opts)
{
    free(opts->label);
    opts->label = NULL;
    free(opts->output_file);
    opts->output_file = NULL;
}

void bmb_options_print_help(const char *prog_name)
{
    fprintf(stdout, "Usage: %s [OPTIONS]\n\n", prog_name);
    fprintf(stdout,
        "  -x, --warmup <n>              untimed calls before measuring (default: %u)\n"
        "  -i, --iterations <n>          timed samples taken (default: %u)\n"
        "  -b, --batch <n>               calls averaged per sample, 0 = choose automatically\n"
        "                                 (default: 0; a batch amortises the clock's own cost)\n"
        "  -v, --vector-size <sweep>     vector sizes for level 1 routines (default: %u)\n"
        "  -m, --matrix-dim1 <sweep>     matrix first dimension for level 2 & 3\n"
        "                                 (default: %u at level 2, %u at level 3)\n"
        "  -M, --matrix-dim2 <sweep>     matrix second dimension for level 2 & 3\n"
        "                                 (default: square matrices, dim2 = dim1 at every point;\n"
        "                                 with -M, every dim1 x dim2 combination is measured)\n"
        "  -t, --thread-count <sweep>    number of BLAS threads (default: %u)\n"
        "  -s, --statistics              add the mean, stddev and slowest sample (default: off)\n"
        "  -c, --verify                  check each point's result against a reference before\n"
        "                                 timing it; stop, exit 2, if it is wrong (default: on)\n"
        "  -C, --no-verify               do not check the results\n"
        "  -L, --layout <order>          matrix storage order, row or col (default: row)\n"
        "  -k, --cache <state>           hot: operands reused from call to call, as in cache;\n"
        "                                 cold: rotated over copies beyond the last-level cache\n"
        "                                 (default: hot)\n"
        "  -a, --align <bytes>           align every operand to bytes, a power of two (default: %u)\n"
        "  -p, --ld-pad <n>              add n elements to every matrix's leading dimension\n"
        "                                 (default: 0)\n"
        "  -w, --warmup-time <s>         repeat calls for s seconds before the first point of\n"
        "                                 each thread count, on top of -x (default: %g)\n"
        "  -T, --min-time <s>            take samples beyond -i until they add up to s seconds\n"
        "                                 (default: 0)\n"
        "  -e, --max-spread <percent>    then take samples until their interquartile range is\n"
        "                                 at most percent of their median (default: 0, off)\n"
        "  -E, --max-time <s>            stop taking samples at s seconds whatever -T and -e ask\n"
        "                                 (default: %g)\n"
        "  -l, --label <text>            tag the results, e.g. \"turbo off\" (default: none)\n"
        "  -o, --output <filename>       also save results to filename\n"
        "  -f, --output-format <fmt>     csv or json (default: csv, or inferred from -o's extension)\n"
        "  -h, --help                    show this help\n"
        "  -V, --version                 show the version, the BLAS backend and the machine\n"
        "\n"
        "A <sweep> is one of:\n"
        "  <max>                a single size            e.g. 4096\n"
        "  <min>:<max>          doubling                 e.g. 256:4096  -> 256 512 1024 2048 4096\n"
        "  <min>:<max>:<step>   linear                   e.g. 1000:4000:1000 -> 1000 2000 3000 4000\n"
        "  <v1>,<v2>,...        exactly these sizes      e.g. 64,1000,4096\n"
        "<max> is always measured, even when the stride would overshoot it.\n"
        "\n"
        "Full documentation: man blas-microbenchmark\n",
        BMB_DEFAULT_WARMUP, BMB_DEFAULT_ITERATIONS, BMB_DEFAULT_VECTOR_SIZE, BMB_DEFAULT_LEVEL2_SIZE,
        BMB_DEFAULT_LEVEL3_SIZE, BMB_DEFAULT_THREADS,
        BMB_DEFAULT_ALIGN, BMB_DEFAULT_WARMUP_TIME, BMB_DEFAULT_MAX_TIME);
}

void bmb_options_print_version(void)
{
    const char *blas = bmb_build_blas_version();
    const bmb_machine_t *m = bmb_machine();
    char line[512];

    printf("blas-microbenchmark %s\n", bmb_build_version());
    if (blas != NULL && blas[0] != '\0') { /* GCOVR_EXCL_BR_LINE: no library this builds against reports an empty version */
        printf("BLAS backend: %s (%s)\n", bmb_build_backend(), blas);
    } else {
        printf("BLAS backend: %s\n", bmb_build_backend());
    }
#if defined(BMB_BUILD_HAS_KERNELS)
    printf("Kernels: %s\n", bmb_build_kernels());
#endif

    /* The machine as well: CI prints this for every job, which is how one
     * finds out what each runner's CPU actually reports. */
    bmb_machine_describe_cpu(m, line, sizeof(line));
    if (line[0] != '\0') { /* GCOVR_EXCL_BR_LINE: sysconf() always gives the CPU count on Linux */
        printf("CPU: %s\n", line);
    }
    bmb_machine_describe_caches(m, line, sizeof(line));
    if (line[0] != '\0') {
        printf("Caches: %s\n", line);
    }
    bmb_machine_describe_frequency(m, line, sizeof(line));
    printf("Frequency: %s\n", (line[0] != '\0') ? line : "not exposed by this system");
    if (m->os[0] != '\0') { /* GCOVR_EXCL_BR_LINE: uname() cannot fail with a valid buffer */
        printf("OS: %s\n", m->os);
    }
}
