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

/* The end of parse_args()'s arguments. A bare NULL may be a plain 0, an
 * int, which va_arg(ap, const char *) must not read. */
#define ARGS_END ((const char *) NULL)

/* Parses a command line given as an argument list ending in ARGS_END. */
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

    if (parse_args(&opts, "-v", spec, ARGS_END) != BMB_OPTIONS_OK) {
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

    if (parse_args(&opts, flag, value, ARGS_END) == BMB_OPTIONS_OK) {
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

    if (parse_args(&opts, "-v", spec, ARGS_END) != BMB_OPTIONS_OK) {
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

    if (parse_args(&opts, ARGS_END) != BMB_OPTIONS_OK) {
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
    } else if (opts.verify != 1) {
        fail("defaults", "--verify is not on");
    } else if (opts.column_major != 0) {
        fail("defaults", "the layout is not row-major");
    } else if (opts.cache_cold != 0) {
        fail("defaults", "the cache is not hot");
    } else if (opts.align != 64 || opts.ld_pad != 0 || opts.ld_pad_set) {
        fail("defaults", "operands are not aligned to 64 bytes, or leading dimensions are padded");
    } else if (opts.warmup_time != 0.2 || opts.min_time != 0.0 || opts.max_spread != 0.0 || opts.max_time != 10.0) {
        fail("defaults", "not 0.2 s of warm-up, no -T or -e, a --max-time of 10 s");
    } else if (opts.vector_size_set || opts.matrix_dim1_set
               || opts.matrix_dim2_set || opts.thread_count_set || opts.layout_set) {
        fail("defaults", "an option is marked as given when none were");
    } else {
        ok("defaults");
    }

    bmb_options_free(&opts);
}

/* Without -M, dim2 is not a range of its own: it follows dim1 point by
 * point (square matrices), which bmb_bench.c decides from matrix_dim2_set.
 * So what the parser must get right is that flag -- and with -M, the range
 * it was given. The pairing itself is checked end to end by
 * test_bmb_square_default.sh. */
static void test_dim2(void)
{
    bmb_options_t opts;
    char dim2[64];

    if (parse_args(&opts, "-m", "8:32", ARGS_END) != BMB_OPTIONS_OK) {
        fail("-m without -M", "-m 8:32 was rejected");
        return;
    }
    if (opts.matrix_dim2_set) {
        fail("-m without -M", "dim2 is marked as given, so the sweep would be a grid");
    } else {
        ok("-m without -M leaves dim2 unset");
    }
    bmb_options_free(&opts);

    if (parse_args(&opts, "-m", "8", "-M", "16:32", ARGS_END) != BMB_OPTIONS_OK) {
        fail("-M given", "-m 8 -M 16:32 was rejected");
        return;
    }
    format_range(&opts.matrix_dim2, dim2, sizeof(dim2));
    if (strcmp(dim2, "16,32") != 0 || !opts.matrix_dim2_set) {
        fail("-M given", "dim2 did not take the given range");
    } else {
        ok("-M sets its own range");
    }
    bmb_options_free(&opts);
}

static void test_output_format(void)
{
    bmb_options_t opts;

    if (parse_args(&opts, "-o", "r.json", ARGS_END) == BMB_OPTIONS_OK) {
        if (opts.output_format != BMB_FORMAT_JSON) {
            fail("-o r.json", "format was not inferred as json");
        } else {
            ok("-o r.json infers json");
        }
        bmb_options_free(&opts);
    } else {
        fail("-o r.json", "rejected");
    }

    if (parse_args(&opts, "-o", "r.json", "-f", "csv", ARGS_END) == BMB_OPTIONS_OK) {
        if (opts.output_format != BMB_FORMAT_CSV) {
            fail("-f csv with -o r.json", "explicit -f did not win");
        } else {
            ok("-f overrides the inferred format");
        }
        bmb_options_free(&opts);
    } else {
        fail("-f csv with -o r.json", "rejected");
    }

    if (parse_args(&opts, "-f", "xml", ARGS_END) == BMB_OPTIONS_OK) {
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
    test_dim2();
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

    /* ---- --label ---- */
    {
        bmb_options_t opts;
        char long_label[256];

        if (parse_args(&opts, ARGS_END) == BMB_OPTIONS_OK) {
            if (opts.label != NULL) {
                fail("no --label", "a label is set although none was given");
            } else {
                ok("no --label leaves the label unset");
            }
            bmb_options_free(&opts);
        }

        if (parse_args(&opts, "--label", "turbo off", ARGS_END) == BMB_OPTIONS_OK
            && opts.label != NULL && strcmp(opts.label, "turbo off") == 0) {
            ok("--label \"turbo off\"");
            bmb_options_free(&opts);
        } else {
            fail("--label \"turbo off\"", "was not stored as given");
        }

        if (parse_args(&opts, "-l", "x", "-l", "y", ARGS_END) == BMB_OPTIONS_OK
            && opts.label != NULL && strcmp(opts.label, "y") == 0) {
            ok("the last --label wins");
            bmb_options_free(&opts);
        } else {
            fail("-l x -l y", "did not keep the last label");
        }

        /* A newline would turn the rest of a text or CSV comment line into
         * data rows. */
        check_count_option("--label", "", 0);
        check_count_option("--label", "two\nlines", 0);
        check_count_option("--label", "tab\there", 0);

        memset(long_label, 'a', 200);
        long_label[200] = '\0';
        check_count_option("--label", long_label, 1);
        memset(long_label, 'a', 201);
        long_label[201] = '\0';
        check_count_option("--label", long_label, 0);
    }

    /* ---- --verify, on by default ---- */
    {
        static const struct {
            const char *a, *b;
            int want;
            const char *what;
        } cases[] = {
            {"-c", NULL, 1, "-c turns the check on"},
            {"--verify", NULL, 1, "--verify turns the check on"},
            {"-C", NULL, 0, "-C turns it off"},
            {"--no-verify", NULL, 0, "--no-verify turns it off"},
            {"-C", "-c", 1, "-C -c: the last one wins, on"},
            {"-c", "-C", 0, "-c -C: the last one wins, off"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;

            if (parse_args(&opts, cases[i].a, cases[i].b, ARGS_END) != BMB_OPTIONS_OK) {
                fail(cases[i].what, "was rejected");
            } else if (opts.verify != cases[i].want) {
                fail(cases[i].what, "left the check in the wrong state");
                bmb_options_free(&opts);
            } else {
                ok(cases[i].what);
                bmb_options_free(&opts);
            }
        }
    }

    /* ---- --layout, row by default ---- */
    {
        static const struct {
            const char *a, *b;
            bmb_options_status_t status;
            int want;
            const char *what;
        } cases[] = {
            {"-L", "col", BMB_OPTIONS_OK, 1, "-L col: column-major"},
            {"--layout", "col", BMB_OPTIONS_OK, 1, "--layout col: column-major"},
            {"--layout", "row", BMB_OPTIONS_OK, 0, "--layout row: row-major"},
            {"--layout", "column", BMB_OPTIONS_ERROR, 0, "--layout column is refused: row or col only"},
            {"--layout", "", BMB_OPTIONS_ERROR, 0, "an empty --layout is refused"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            bmb_options_status_t st = parse_args(&opts, cases[i].a, cases[i].b, ARGS_END);

            if (st != cases[i].status) {
                fail(cases[i].what, "got the wrong status");
            } else if (st == BMB_OPTIONS_OK && (opts.column_major != cases[i].want || !opts.layout_set)) {
                fail(cases[i].what, "left the layout in the wrong state");
            } else {
                ok(cases[i].what);
            }
            if (st == BMB_OPTIONS_OK) {
                bmb_options_free(&opts);
            }
        }
    }

    /* ---- --cache, hot by default (#160) ---- */
    {
        static const struct {
            const char *a, *b, *c, *d;
            bmb_options_status_t status;
            int want;
            const char *what;
        } cases[] = {
            {"-k", "cold", ARGS_END, NULL, BMB_OPTIONS_OK, 1, "-k cold: cold"},
            {"--cache", "cold", ARGS_END, NULL, BMB_OPTIONS_OK, 1, "--cache cold: cold"},
            {"--cache", "hot", ARGS_END, NULL, BMB_OPTIONS_OK, 0, "--cache hot: hot"},
            {"-k", "cold", "-k", "hot", BMB_OPTIONS_OK, 0, "the last --cache given wins"},
            {"--cache", "warm", ARGS_END, NULL, BMB_OPTIONS_ERROR, 0, "--cache warm is refused: hot or cold only"},
            {"--cache", "", ARGS_END, NULL, BMB_OPTIONS_ERROR, 0, "an empty --cache is refused"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            bmb_options_status_t st = parse_args(&opts, cases[i].a, cases[i].b, cases[i].c, cases[i].d, ARGS_END);

            if (st != cases[i].status) {
                fail(cases[i].what, "got the wrong status");
            } else if (st == BMB_OPTIONS_OK && opts.cache_cold != cases[i].want) {
                fail(cases[i].what, "left the cache in the wrong state");
            } else {
                ok(cases[i].what);
            }
            if (st == BMB_OPTIONS_OK) {
                bmb_options_free(&opts);
            }
        }
    }

    /* ---- --align and --ld-pad (#163) ---- */
    {
        static const struct {
            const char *a, *b;
            bmb_options_status_t status;
            unsigned int align, pad;
            const char *what;
        } cases[] = {
            {"-a", "4096", BMB_OPTIONS_OK, 4096, 0, "-a 4096: a page"},
            {"--align", "8", BMB_OPTIONS_OK, 8, 0, "--align 8, a pointer's size, the least"},
            {"--align", "2097152", BMB_OPTIONS_OK, 2097152, 0, "--align 2097152, a huge page, the most"},
            {"--align", "4", BMB_OPTIONS_ERROR, 0, 0, "--align 4 is refused: less than a pointer"},
            {"--align", "4194304", BMB_OPTIONS_ERROR, 0, 0, "--align 4194304 is refused: past a huge page"},
            {"--align", "96", BMB_OPTIONS_ERROR, 0, 0, "--align 96 is refused: not a power of two"},
            {"--align", "-64", BMB_OPTIONS_ERROR, 0, 0, "--align -64 is refused"},
            {"--align", "", BMB_OPTIONS_ERROR, 0, 0, "an empty --align is refused"},
            {"-p", "8", BMB_OPTIONS_OK, 64, 8, "-p 8"},
            {"--ld-pad", "0", BMB_OPTIONS_OK, 64, 0, "--ld-pad 0, given"},
            {"--ld-pad", "65536", BMB_OPTIONS_OK, 64, 65536, "--ld-pad 65536, the most"},
            {"--ld-pad", "65537", BMB_OPTIONS_ERROR, 0, 0, "--ld-pad 65537 is refused"},
            {"--ld-pad", "-1", BMB_OPTIONS_ERROR, 0, 0, "--ld-pad -1 is refused"},
            {"--ld-pad", "x", BMB_OPTIONS_ERROR, 0, 0, "--ld-pad x is refused"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            bmb_options_status_t st = parse_args(&opts, cases[i].a, cases[i].b, ARGS_END);

            if (st != cases[i].status) {
                fail(cases[i].what, "got the wrong status");
            } else if (st == BMB_OPTIONS_OK
                       && (opts.align != cases[i].align || opts.ld_pad != cases[i].pad
                           || opts.ld_pad_set != (cases[i].a[1] == 'p' || cases[i].a[2] == 'l'))) {
                fail(cases[i].what, "left the alignment or the padding in the wrong state");
            } else {
                ok(cases[i].what);
            }
            if (st == BMB_OPTIONS_OK) {
                bmb_options_free(&opts);
            }
        }
    }

    /* ---- --warmup-time, --min-time, --max-spread, --max-time (#164) ---- */
    {
        static const struct {
            const char *a, *b;
            bmb_options_status_t status;
            double want;
            const char *what;
        } cases[] = {
            {"-w", "0", BMB_OPTIONS_OK, 0.0, "-w 0: no warm-up"},
            {"--warmup-time", "0.5", BMB_OPTIONS_OK, 0.5, "--warmup-time 0.5"},
            {"--warmup-time", "3600", BMB_OPTIONS_OK, 3600.0, "--warmup-time 3600, the most"},
            {"--warmup-time", "3600.5", BMB_OPTIONS_ERROR, 0.0, "--warmup-time 3600.5 is refused"},
            {"--warmup-time", "-1", BMB_OPTIONS_ERROR, 0.0, "--warmup-time -1 is refused: no sign"},
            {"--warmup-time", "1e3", BMB_OPTIONS_ERROR, 0.0, "--warmup-time 1e3 is refused: no exponent"},
            {"--warmup-time", ".5", BMB_OPTIONS_ERROR, 0.0, "--warmup-time .5 is refused: a digit first"},
            {"--warmup-time", "5.", BMB_OPTIONS_ERROR, 0.0, "--warmup-time 5. is refused: a digit after the point"},
            {"--warmup-time", "inf", BMB_OPTIONS_ERROR, 0.0, "--warmup-time inf is refused"},
            {"--warmup-time", "nan", BMB_OPTIONS_ERROR, 0.0, "--warmup-time nan is refused"},
            {"--warmup-time", "0x1", BMB_OPTIONS_ERROR, 0.0, "--warmup-time 0x1 is refused"},
            {"--warmup-time", "", BMB_OPTIONS_ERROR, 0.0, "an empty --warmup-time is refused"},
            {"-T", "1.25", BMB_OPTIONS_OK, 1.25, "-T 1.25"},
            {"-e", "2.5", BMB_OPTIONS_OK, 2.5, "-e 2.5"},
            {"--max-spread", "100", BMB_OPTIONS_OK, 100.0, "--max-spread 100, the most"},
            {"--max-spread", "100.5", BMB_OPTIONS_ERROR, 0.0, "--max-spread 100.5 is refused"},
            {"-E", "0.5", BMB_OPTIONS_OK, 0.5, "-E 0.5"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            bmb_options_status_t st = parse_args(&opts, cases[i].a, cases[i].b, ARGS_END);
            double got = 0.0;

            if (st == BMB_OPTIONS_OK) {
                got = (cases[i].a[1] == 'w' || cases[i].a[2] == 'w') ? opts.warmup_time
                    : (cases[i].a[1] == 'T') ? opts.min_time
                    : (cases[i].a[1] == 'e' || cases[i].a[2] == 'm') ? opts.max_spread
                    : opts.max_time;
            }
            if (st != cases[i].status) {
                fail(cases[i].what, "got the wrong status");
            } else if (st == BMB_OPTIONS_OK && got != cases[i].want) {
                fail(cases[i].what, "stored another value");
            } else {
                ok(cases[i].what);
            }
            if (st == BMB_OPTIONS_OK) {
                bmb_options_free(&opts);
            }
        }
    }
    {
        bmb_options_t opts;

        if (parse_args(&opts, "-T", "20", ARGS_END) == BMB_OPTIONS_ERROR) {
            ok("-T 20 past the default --max-time of 10 is refused");
        } else {
            fail("-T 20", "was accepted past --max-time 10");
            bmb_options_free(&opts);
        }
        if (parse_args(&opts, "-T", "20", "-E", "20", ARGS_END) == BMB_OPTIONS_OK) {
            ok("-T 20 -E 20: at --max-time, accepted");
            bmb_options_free(&opts);
        } else {
            fail("-T 20 -E 20", "was refused");
        }
    }

    /* A matrix size plus the padding within the ceiling: 1518500249 - 8. */
    {
        static const struct {
            const char *dim, *sizes;
            bmb_options_status_t status;
            const char *what;
        } cases[] = {
            {"-m", "1518500241", BMB_OPTIONS_OK, "-p 8 -m 1518500241: padded, at the ceiling"},
            {"-m", "1518500242", BMB_OPTIONS_ERROR, "-p 8 -m 1518500242 is refused: padded, past it"},
            {"-m", "1518500242,64", BMB_OPTIONS_ERROR, "-p 8 -m 1518500242,64: the largest point, not the last"},
            {"-M", "1518500242", BMB_OPTIONS_ERROR, "-p 8 -M 1518500242 is refused too"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            bmb_options_status_t st = parse_args(&opts, "-p", "8", cases[i].dim, cases[i].sizes, ARGS_END);

            if (st != cases[i].status) {
                fail(cases[i].what, "got the wrong status");
            } else {
                ok(cases[i].what);
            }
            if (st == BMB_OPTIONS_OK) {
                bmb_options_free(&opts);
            }
        }
    }

    /* ---- provenance ---- */
    {
        bmb_options_t opts;

        if (parse_args(&opts, "-V", ARGS_END) != BMB_OPTIONS_VERSION) {
            fail("-V", "did not report BMB_OPTIONS_VERSION");
        } else {
            ok("-V");
        }
        if (parse_args(&opts, "--version", ARGS_END) != BMB_OPTIONS_VERSION) {
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

    /* ---- the backend a FlexiBLAS build names, from a name it read ---- */
    {
        static const struct {
            const char *library, *want;
            size_t size;
            const char *what;
        } cases[] = {
            {"OPENBLAS-OPENMP", "flexiblas/openblas-openmp", 64, "FlexiBLAS: the loaded library, lower-cased"},
            {"MY\"LIB x/y\n", "flexiblas/my-lib-x-y-", 64,
             "FlexiBLAS: a quote, a space, a slash, a newline from a config file become -"},
            {"", "flexiblas/unknown", 64, "FlexiBLAS: no name, unknown"},
            {"BLIS-OPENMP", "flexiblas/bli", 14, "FlexiBLAS: cut to the buffer, still terminated"},
            {"", "flexiblas/", 12, "FlexiBLAS: no name and no room for unknown, the prefix alone"},
            {"BLIS-OPENMP", "", 5, "FlexiBLAS: no room for the prefix, an empty name"},
        };
        size_t i;
        char untouched[] = "x";

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            char buf[64];

            bmb_build_backend_name(cases[i].library, buf, cases[i].size);
            if (strcmp(buf, cases[i].want) != 0) {
                fail(cases[i].what, buf);
            } else {
                ok(cases[i].what);
            }
        }

        bmb_build_backend_name("BLIS-OPENMP", untouched, 0);
        if (strcmp(untouched, "x") != 0) {
            fail("FlexiBLAS: a buffer of 0 bytes", "was written to");
        } else {
            ok("FlexiBLAS: a buffer of 0 bytes is left alone");
        }
    }

    /* ---- thread counts share the sweep parser ---- */
    {
        bmb_options_t opts;
        char got[64];

        if (parse_args(&opts, "-t", "1,2,4", ARGS_END) == BMB_OPTIONS_OK) {
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

    /* ---- every sweep option refuses what -v refuses ---- */
    {
        static const char *const flags[] = {"-v", "-m", "-M", "-t"};
        static const char *const bad[] = {"0", "1518500250", "1:257:1", "64,1518500250"};
        size_t f, b;

        for (f = 0; f < sizeof(flags) / sizeof(flags[0]); f++) {
            for (b = 0; b < sizeof(bad) / sizeof(bad[0]); b++) {
                bmb_options_t opts;
                char what[64];

                snprintf(what, sizeof(what), "%s %s is refused", flags[f], bad[b]);
                if (parse_args(&opts, flags[f], bad[b], ARGS_END) == BMB_OPTIONS_OK) {
                    fail(what, "was accepted");
                    bmb_options_free(&opts);
                } else {
                    ok(what);
                }
            }
        }
    }

    /* ---- the edges of the command line ---- */
    {
        bmb_options_t opts;
        char list[2 * 257];
        char longspec[600];
        size_t v;

        /* 257 points: past the cap, and so past the 511 characters a
         * sweep may have, whichever is checked first. */
        for (v = 0; v < 257; v++) {
            list[2 * v] = '1';
            list[2 * v + 1] = ',';
        }
        list[sizeof(list) - 1] = '\0';
        check_sweep(list, NULL);
        memset(longspec, '1', sizeof(longspec) - 1);
        longspec[sizeof(longspec) - 1] = '\0';
        check_sweep(longspec, NULL);   /* longer than any sweep can be */

        check_count_option("--label", "del\x7f", 0);   /* DEL is a control character */

        if (parse_args(&opts, "-o", "r.j", ARGS_END) == BMB_OPTIONS_OK
            && opts.output_format == BMB_FORMAT_CSV) {
            ok("-o r.j: a name shorter than .json is CSV");
            bmb_options_free(&opts);
        } else {
            fail("-o r.j", "was not taken as CSV");
        }

        if (parse_args(&opts, "-m", ARGS_END) == BMB_OPTIONS_ERROR) {
            ok("-m with no value is refused");
        } else {
            fail("-m with no value", "was accepted");
            bmb_options_free(&opts);
        }
        if (parse_args(&opts, "--no-such-option", ARGS_END) == BMB_OPTIONS_ERROR) {
            ok("an unknown option is refused");
        } else {
            fail("--no-such-option", "was accepted");
            bmb_options_free(&opts);
        }
    }

    /* ---- how a sweep is described in a warning ---- */
    {
        static const struct {
            const char *spec;
            size_t size;
            const char *want;
            const char *what;
        } cases[] = {
            {"4", 64, "4", "describe: one point"},
            {"1,2,4,8", 64, "1, 2, 4, 8", "describe: four points, all shown"},
            {"1:256", 64, "1, 2, 4, 8, ... (9 points)", "describe: more than four, elided with the count"},
            {"1000,2000", 8, "1000", "describe: a point that does not fit is left out, still terminated"},
            {"1000,2000", 5, "1000", "describe: an exact fit"},
            {"1000", 3, "", "describe: not even the first point fits, empty"},
            {"1:256", 20, "1, 2, 4, 8", "describe: no room for the count, the points alone"},
        };
        size_t i;

        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
            bmb_options_t opts;
            char buf[64];

            if (parse_args(&opts, "-v", cases[i].spec, ARGS_END) != BMB_OPTIONS_OK) {
                fail(cases[i].what, "the sweep was rejected");
                continue;
            }
            memset(buf, 'z', sizeof(buf));
            bmb_range_describe(&opts.vector_size, buf, cases[i].size);
            if (memchr(buf, '\0', cases[i].size) == NULL || strcmp(buf, cases[i].want) != 0) {
                fail(cases[i].what, buf);
            } else {
                ok(cases[i].what);
            }
            bmb_options_free(&opts);
        }

        {
            bmb_range_t one;
            char untouched[] = "x";

            bmb_range_set_single(&one, 7);
            bmb_range_describe(&one, untouched, 0);
            if (strcmp(untouched, "x") != 0) {
                fail("describe: a buffer of 0 bytes", "was written to");
            } else {
                ok("describe: a buffer of 0 bytes is left alone");
            }
        }
    }

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
