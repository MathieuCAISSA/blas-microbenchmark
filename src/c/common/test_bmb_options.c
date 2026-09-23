/* Unit tests for the command-line layer.
 *
 * Everything here goes through bmb_options_parse(), so what is tested is
 * what a benchmark actually gets, not an internal helper. This is the
 * densest logic in the project -- four sweep forms, two ceilings, a point
 * cap -- and the place a silent bug hurts most: a sweep that quietly drops
 * its endpoint, or a size that truncates to zero, produces numbers that
 * look perfectly reasonable.
 *
 * Nothing here runs a benchmark, so sizes near the ceiling are safe to
 * test: they are parsed, never allocated. */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "bmb_build.h"
#include "bmb_options.h"

/* Wide enough for the longest expansion a sweep can produce: 256 points
 * of up to ten digits, plus separators. */
#define BMB_TEST_BUF 4096

static int failures;

static void fail(const char *what, const char *detail)
{
    printf("FAIL %s: %s\n", what, detail);
    failures++;
}

static void ok(const char *what)
{
    printf("ok   %s\n", what);
}

/* Parses a command line given as a NULL-terminated argument list. */
static bmb_options_status_t parse_args(bmb_options_t *opts, ...)
{
    char *argv[16];
    int argc = 0;
    va_list ap;
    const char *arg;

    argv[argc++] = (char *) "bmb_test";

    va_start(ap, opts);
    while ((arg = va_arg(ap, const char *)) != NULL) {
        if (argc >= (int) (sizeof(argv) / sizeof(argv[0])) - 1) {
            break;
        }
        argv[argc++] = (char *) arg;
    }
    va_end(ap);
    argv[argc] = NULL;

    return bmb_options_parse(argc, argv, opts);
}

static void format_range(const bmb_range_t *range, char *buf, size_t size)
{
    size_t used = 0;
    size_t i;

    buf[0] = '\0';
    for (i = 0; i < range->count; i++) {
        int n = snprintf(buf + used, size - used, "%s%zu", (i == 0) ? "" : ",",
                         range->values[i]);

        if (n < 0 || (size_t) n >= size - used) {
            return;
        }
        used += (size_t) n;
    }
}

/* Checks that `-v spec` expands to exactly `expected` (comma-separated),
 * or that it is rejected when `expected` is NULL. */
static void check_sweep(const char *spec, const char *expected)
{
    bmb_options_t opts;
    char got[BMB_TEST_BUF];
    char what[128];

    snprintf(what, sizeof(what), "-v %.100s", spec);

    if (parse_args(&opts, "-v", spec, NULL) != BMB_OPTIONS_OK) {
        if (expected == NULL) {
            ok(what);
        } else {
            fail(what, "rejected, should have been accepted");
        }
        return;
    }

    if (expected == NULL) {
        format_range(&opts.vector_size, got, sizeof(got));
        fail(what, "accepted, should have been rejected");
        bmb_options_free(&opts);
        return;
    }

    format_range(&opts.vector_size, got, sizeof(got));
    if (strcmp(got, expected) != 0) {
        char detail[2 * BMB_TEST_BUF + 64];

        snprintf(detail, sizeof(detail), "expands to %s, expected %s", got, expected);
        fail(what, detail);
    } else {
        ok(what);
    }
    bmb_options_free(&opts);
}

static void check_count_option(const char *flag, const char *value, int accepted)
{
    bmb_options_t opts;
    char what[128];

    snprintf(what, sizeof(what), "%s %.60s", flag, value);

    if (parse_args(&opts, flag, value, NULL) == BMB_OPTIONS_OK) {
        if (accepted) {
            ok(what);
        } else {
            fail(what, "accepted, should have been rejected");
        }
        bmb_options_free(&opts);
    } else {
        if (accepted) {
            fail(what, "rejected, should have been accepted");
        } else {
            ok(what);
        }
    }
}

static void check_point_count(const char *spec, size_t expected)
{
    bmb_options_t opts;
    char what[128];
    char detail[128];

    snprintf(what, sizeof(what), "-v %.100s point count", spec);

    if (parse_args(&opts, "-v", spec, NULL) != BMB_OPTIONS_OK) {
        fail(what, "rejected, should have been accepted");
        return;
    }
    if (opts.vector_size.count != expected) {
        snprintf(detail, sizeof(detail), "got %zu points, expected %zu",
                 opts.vector_size.count, expected);
        fail(what, detail);
    } else {
        ok(what);
    }
    bmb_options_free(&opts);
}

static void test_defaults(void)
{
    bmb_options_t opts;
    char got[64];

    if (parse_args(&opts, NULL) != BMB_OPTIONS_OK) {
        fail("defaults", "an empty command line was rejected");
        return;
    }

    format_range(&opts.vector_size, got, sizeof(got));
    if (strcmp(got, "4096") != 0) {
        fail("defaults", "vector size is not 4096");
    } else if (opts.warmup != 1 || opts.iterations != 10) {
        fail("defaults", "warmup/iterations are not 1/10");
    } else if (opts.thread_count.count != 1 || opts.thread_count.values[0] != 1) {
        fail("defaults", "thread count is not a single 1");
    } else if (opts.statistics != 0 || opts.output_file != NULL) {
        fail("defaults", "statistics/output are not off");
    } else if (opts.vector_size_set || opts.matrix_dim1_set
               || opts.matrix_dim2_set || opts.thread_count_set) {
        fail("defaults", "an option is marked as given when none were");
    } else {
        ok("defaults");
    }

    bmb_options_free(&opts);
}

/* --matrix-dim2 defaults to --matrix-dim1 so that a routine with two
 * dimensions measures square matrices unless told otherwise. */
static void test_dim2_mirrors_dim1(void)
{
    bmb_options_t opts;
    char dim1[64];
    char dim2[64];

    if (parse_args(&opts, "-m", "8:32", NULL) != BMB_OPTIONS_OK) {
        fail("-M defaults to -m", "-m 8:32 was rejected");
        return;
    }
    format_range(&opts.matrix_dim1, dim1, sizeof(dim1));
    format_range(&opts.matrix_dim2, dim2, sizeof(dim2));
    if (strcmp(dim1, dim2) != 0) {
        fail("-M defaults to -m", "dim2 does not mirror dim1");
    } else if (opts.matrix_dim2_set) {
        fail("-M defaults to -m", "dim2 is marked as explicitly given");
    } else {
        ok("-M defaults to -m");
    }
    bmb_options_free(&opts);

    if (parse_args(&opts, "-m", "8", "-M", "16", NULL) != BMB_OPTIONS_OK) {
        fail("-M overrides -m", "-m 8 -M 16 was rejected");
        return;
    }
    format_range(&opts.matrix_dim2, dim2, sizeof(dim2));
    if (strcmp(dim2, "16") != 0 || !opts.matrix_dim2_set) {
        fail("-M overrides -m", "dim2 did not take the given value");
    } else {
        ok("-M overrides -m");
    }
    bmb_options_free(&opts);
}

static void test_output_format(void)
{
    bmb_options_t opts;

    if (parse_args(&opts, "-o", "r.json", NULL) == BMB_OPTIONS_OK) {
        if (opts.output_format != BMB_FORMAT_JSON) {
            fail("-o r.json", "format was not inferred as json");
        } else {
            ok("-o r.json infers json");
        }
        bmb_options_free(&opts);
    } else {
        fail("-o r.json", "rejected");
    }

    if (parse_args(&opts, "-o", "r.json", "-f", "csv", NULL) == BMB_OPTIONS_OK) {
        if (opts.output_format != BMB_FORMAT_CSV) {
            fail("-f csv with -o r.json", "explicit -f did not win");
        } else {
            ok("-f overrides the inferred format");
        }
        bmb_options_free(&opts);
    } else {
        fail("-f csv with -o r.json", "rejected");
    }

    if (parse_args(&opts, "-f", "xml", NULL) == BMB_OPTIONS_OK) {
        fail("-f xml", "accepted, should have been rejected");
        bmb_options_free(&opts);
    } else {
        ok("-f xml");
    }
}

int main(void)
{
    char spec[64];

    test_defaults();
    test_dim2_mirrors_dim1();
    test_output_format();

    /* ---- the four sweep forms ---- */
    check_sweep("512", "512");
    check_sweep("256:4096", "256,512,1024,2048,4096");
    check_sweep("1000:4000:1000", "1000,2000,3000,4000");
    check_sweep("64,1000,4096", "64,1000,4096");

    /* The endpoint must be measured whatever the stride does -- this is
     * the bug that shipped in 0.3.0 and was fixed in 0.4.0. */
    check_sweep("100:1000", "100,200,400,800,1000");
    check_sweep("100:1000:300", "100,400,700,1000");
    check_sweep("100:1000:400", "100,500,900,1000");   /* last step is short */
    check_sweep("1:3", "1,2,3");
    check_sweep("100:200:1000", "100,200");   /* step overshoots immediately */
    check_sweep("512:512", "512");            /* min == max */
    check_sweep("7", "7");                    /* a list of one is just a point */

    /* ---- malformed ---- */
    check_sweep("1000:100", NULL);            /* min > max */
    check_sweep("0", NULL);
    check_sweep("0,64", NULL);
    check_sweep("1:10:0", NULL);              /* a zero step never advances */
    check_sweep("-5", NULL);
    check_sweep("abc", NULL);
    check_sweep("5abc", NULL);
    check_sweep("", NULL);
    check_sweep("64,,128", NULL);
    check_sweep(",64", NULL);
    check_sweep("64,", NULL);
    check_sweep("1:2:3:4", NULL);             /* one field too many */
    check_sweep("1,2:3", NULL);               /* a list has no endpoints */
    check_sweep(":", NULL);
    check_sweep("1:", NULL);

    /* ---- the two ceilings ---- */
    check_sweep("1518500249", "1518500249");  /* largest size that cannot wrap */
    check_sweep("1518500250", NULL);          /* smallest whose square wraps */
    check_sweep("2147483648", NULL);          /* past INT_MAX */
    check_sweep("99999999999999999999", NULL);/* past unsigned long long */

    /* ---- the point cap ---- */
    check_point_count("1:256:1", 256);
    check_sweep("1:257:1", NULL);
    snprintf(spec, sizeof(spec), "1:%d", 1 << 20);
    check_point_count(spec, 21);              /* doubling stays well inside */

    /* ---- plain counts ---- */
    check_count_option("-x", "0", 1);         /* no warmup is legitimate */
    check_count_option("-x", "5", 1);
    check_count_option("-x", "abc", 0);
    check_count_option("-x", "5abc", 0);
    check_count_option("-x", "-5", 0);
    check_count_option("-x", "", 0);
    check_count_option("-i", "1", 1);
    check_count_option("-b", "0", 1);          /* 0 means "choose for me" */
    check_count_option("-b", "1", 1);          /* 1 restores per-call timing */
    check_count_option("-b", "abc", 0);
    check_count_option("-b", "-1", 0);
    check_count_option("-i", "0", 0);         /* nothing would be measured */
    check_count_option("-i", "-1", 0);

    /* ---- provenance ---- */
    {
        bmb_options_t opts;

        if (parse_args(&opts, "-V", NULL) != BMB_OPTIONS_VERSION) {
            fail("-V", "did not report BMB_OPTIONS_VERSION");
        } else {
            ok("-V");
        }
        if (parse_args(&opts, "--version", NULL) != BMB_OPTIONS_VERSION) {
            fail("--version", "did not report BMB_OPTIONS_VERSION");
        } else {
            ok("--version");
        }

        /* A build that cannot name its backend cannot label its results,
         * so an empty or "unknown" name is a configure bug, not a
         * cosmetic one. */
        if (bmb_build_version()[0] == '\0') {
            fail("build version", "is empty");
        } else {
            ok("build version");
        }
        if (strcmp(bmb_build_backend(), "unknown") == 0
            || bmb_build_backend()[0] == '\0') {
            fail("build backend", "configure did not resolve a backend name");
        } else {
            ok("build backend");
        }
    }

    /* ---- thread counts share the sweep parser ---- */
    {
        bmb_options_t opts;
        char got[64];

        if (parse_args(&opts, "-t", "1,2,4", NULL) == BMB_OPTIONS_OK) {
            format_range(&opts.thread_count, got, sizeof(got));
            if (strcmp(got, "1,2,4") != 0 || !opts.thread_count_set) {
                fail("-t 1,2,4", "did not expand to 1,2,4");
            } else {
                ok("-t 1,2,4");
            }
            bmb_options_free(&opts);
        } else {
            fail("-t 1,2,4", "rejected");
        }
    }

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
