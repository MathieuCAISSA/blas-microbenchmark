/* Fuzzes the command-line parser: each input, split on its NUL bytes,
 * is an argv given to bmb_options_parse(), and whatever it accepts is
 * held to what the benchmarks rely on -- every point between 1 and the
 * size ceiling, at most BMB_MAX_SWEEP_POINTS of them, at least one
 * iteration, a printable label, the output format a .json file implies,
 * an alignment posix_memalign() takes and a padding that keeps every
 * matrix size within the ceiling (#163), times within an hour, a spread
 * within 100 %, a --min-time not past --max-time (#164) -- then described
 * into buffers of every small size. A crash, a
 * sanitizer report or a broken promise aborts.
 *
 * Built two ways. With -DBMB_LIBFUZZER it is a libFuzzer target, which
 * ClusterFuzzLite builds and runs (.clusterfuzzlite/). Otherwise it has a
 * main of its own that replays the seed corpus in fuzz/bmb_options/, or
 * the files it is given, through the same function: make check runs it,
 * with any compiler, so the target cannot fall out of step with the
 * parser. */

#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <getopt.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bmb_options.h"

#define FUZZ_MAX_ARGS 64

int LLVMFuzzerInitialize(int *argc, char ***argv);
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

static void check(int holds, const char *what)
{
    if (!holds) {
        fprintf(stderr, "fuzz_bmb_options: %s\n", what);
        abort();
    }
}

/* The ceiling bmb_options.c applies, worked out another way than its
 * bisection: the largest d up to INT_MAX for which d * d doubles can be
 * sized, as the integer square root of the elements size_t can count
 * (Newton's method, from above). */
static size_t size_ceiling(void)
{
    const size_t elements = SIZE_MAX / sizeof(double);
    size_t x = elements;
    size_t y = x / 2 + 1;

    while (y < x) {
        x = y;
        y = (x + elements / x) / 2;
    }
    return x < (size_t) INT_MAX ? x : (size_t) INT_MAX;
}

static void check_range(const bmb_range_t *range)
{
    static size_t ceiling;
    char buf[80];
    size_t i;
    size_t size;

    if (ceiling == 0) {
        ceiling = size_ceiling();
    }

    check(range->count >= 1, "a range with no point");
    check(range->count <= BMB_MAX_SWEEP_POINTS, "a range with too many points");
    for (i = 0; i < range->count; i++) {
        check(range->values[i] >= 1, "a point of 0");
        check(range->values[i] <= ceiling, "a point above the size ceiling");
    }

    for (size = 0; size <= sizeof(buf); size++) {
        memset(buf, 'x', sizeof(buf));
        bmb_range_describe(range, buf, size);
        if (size > 0) {
            check(memchr(buf, '\0', size) != NULL, "a description not terminated within its buffer");
        }
        check(size == sizeof(buf) || buf[size] == 'x', "a description written past its buffer");
    }
}

static size_t max_point(const bmb_range_t *range)
{
    size_t i, max = 0;

    for (i = 0; i < range->count; i++) {
        max = (range->values[i] > max) ? range->values[i] : max;
    }
    return max;
}

static void check_options(const bmb_options_t *opts)
{
    check_range(&opts->vector_size);
    check_range(&opts->matrix_dim1);
    check_range(&opts->matrix_dim2);
    check_range(&opts->matrix_dim3);
    check_range(&opts->thread_count);

    check(opts->iterations >= 1, "no iteration");
    check(strspn(opts->trans, "NTC") == strlen(opts->trans) && strlen(opts->trans) <= 2
          && (opts->uplo == 0 || opts->uplo == 'U' || opts->uplo == 'L')
          && (opts->side == 0 || opts->side == 'L' || opts->side == 'R')
          && (opts->diag == 0 || opts->diag == 'N' || opts->diag == 'U'),
          "an argument letter a routine cannot be given");
    check(opts->align >= sizeof(void *) && (opts->align & (opts->align - 1)) == 0,
          "an alignment posix_memalign() refuses");
    check(opts->warmup_time >= 0.0 && opts->warmup_time <= 3600.0 && opts->min_time >= 0.0
          && opts->min_time <= opts->max_time && opts->max_time <= 3600.0,
          "a time out of bounds, or a --min-time past --max-time");
    check(opts->max_spread >= 0.0 && opts->max_spread <= 100.0, "a spread out of bounds");
    check(max_point(&opts->matrix_dim1) + opts->ld_pad <= size_ceiling()
          && max_point(&opts->matrix_dim2) + opts->ld_pad <= size_ceiling()
          && max_point(&opts->matrix_dim3) + opts->ld_pad <= size_ceiling(),
          "a matrix size, padded, past the size ceiling");

    if (opts->label != NULL) {
        size_t len = strlen(opts->label);
        size_t i;

        check(len >= 1 && len <= 200, "a label of the wrong length");
        for (i = 0; i < len; i++) {
            unsigned char c = (unsigned char) opts->label[i];

            check(c >= 0x20 && c != 0x7f, "a control character in the label");
        }
    }

    if (opts->output_file != NULL && !opts->output_format_set) {
        size_t len = strlen(opts->output_file);
        int json = len >= 5 && strcmp(opts->output_file + len - 5, ".json") == 0;

        check(json == (opts->output_format == BMB_FORMAT_JSON),
              "an output format other than the file name's");
    }
}

int LLVMFuzzerInitialize(int *argc, char ***argv)
{
    (void) argc;
    (void) argv;
    /* --help and every error print the usage: not what is being tested. */
    if (freopen("/dev/null", "w", stdout) == NULL) {
        abort();
    }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    static char prog[] = "fuzz_bmb_options";
    static char *none[] = {prog, NULL};
    char *argv[FUZZ_MAX_ARGS + 1];
    char *text = malloc(size + 1);
    bmb_options_t opts;
    int argc = 0;
    size_t i = 0;

    if (text == NULL) {
        return 0;
    }
    if (size > 0) {
        memcpy(text, data, size);
    }
    text[size] = '\0';

    argv[argc++] = prog;
    while (i <= size && argc < FUZZ_MAX_ARGS) {
        argv[argc++] = text + i;
        i += strlen(text + i) + 1;
    }
    argv[argc] = NULL;

    /* glibc's getopt keeps a pointer into the last argv when a parse stops
     * inside a cluster of short options (-hs stops at -h), and setting
     * optind to 1, as the parser does, does not drop it: it would read the
     * previous input, freed. A benchmark parses once; here, optind = 0
     * makes getopt start afresh. */
    optind = 0;
    while (getopt_long(1, none, "", NULL, NULL) != -1) {
    }

    if (bmb_options_parse(argc, argv, &opts) == BMB_OPTIONS_OK) {
        check_options(&opts);
    }
    bmb_options_free(&opts);
    free(text);
    return 0;
}

#ifndef BMB_LIBFUZZER

static int replay_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char buf[4096];
    size_t n;

    if (f == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        return -1;
    }
    n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    LLVMFuzzerTestOneInput(buf, n);
    fprintf(stderr, "ok   %s\n", path);
    return 0;
}

int main(int argc, char *argv[])
{
    const char *srcdir = getenv("srcdir");
    char dir[4096];
    char path[8192];
    DIR *d;
    const struct dirent *e;
    int replayed = 0;
    int i;

    LLVMFuzzerInitialize(&argc, &argv);

    if (argc > 1) {
        for (i = 1; i < argc; i++) {
            if (replay_file(argv[i]) != 0) {
                return 1;
            }
        }
        return 0;
    }

    snprintf(dir, sizeof(dir), "%s/fuzz/bmb_options", srcdir != NULL ? srcdir : ".");
    d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "FAIL: no seed corpus in %s\n", dir);
        return 1;
    }
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, e->d_name);
        if (replay_file(path) != 0) {
            closedir(d);
            return 1;
        }
        replayed++;
    }
    closedir(d);

    /* An empty or half-copied corpus must not pass as a fuzzed parser. */
    if (replayed < 10) {
        fprintf(stderr, "FAIL: only %d seed(s) in %s\n", replayed, dir);
        return 1;
    }
    fprintf(stderr, "all %d seeds replayed\n", replayed);
    return 0;
}

#endif
