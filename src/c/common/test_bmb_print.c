/* Checks what the output layer makes of inputs no benchmark gives it: a
 * CSV column name rewritten from a label with every kind of character
 * around its words (punctuation first, braces, brackets, @, ~, digits),
 * the columns of a routine with no operation count, a command line whose
 * words a shell would need quoted (a quote, a newline, a DEL, an empty
 * word) and a variable's value with a space, in the run lines and the
 * JSON, and a stream already in error, or whose flush fails, when its
 * writes are checked. The rest of the output is checked end to end by
 * test_bmb_output_formats.sh. */

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bmb_print.h"
#include "bmb_result.h"

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

/* A device opened for writing, never created. */
static FILE *open_device(const char *path)
{
    int fd = open(path, O_WRONLY);
    FILE *f = (fd >= 0) ? fdopen(fd, "w") : NULL;

    if (f == NULL && fd >= 0) {
        close(fd);
    }
    return f;
}

/* The first two lines that are not comments: the header and a row. */
static void csv_of(const bmb_result_set_t *rs, char *header, char *row, size_t size)
{
    FILE *f = tmpfile();
    char line[2048];
    int n = 0;

    header[0] = row[0] = '\0';
    if (f == NULL) {
        return;
    }
    bmb_print_csv(f, rs);
    rewind(f);
    while (n < 2 && fgets(line, sizeof(line), f) != NULL) {
        if (line[0] == '#') {
            continue;
        }
        line[strcspn(line, "\n")] = '\0';
        snprintf(n == 0 ? header : row, size, "%.*s", (int) size - 1, line);
        n++;
    }
    fclose(f);
}

/* Whether what `print` writes for rs has a line equal to `line`, or, with
 * `json`, a line containing it. */
static int printed(void (*print)(FILE *, const bmb_result_set_t *), const bmb_result_set_t *rs,
                   const char *line, int json)
{
    FILE *f = tmpfile();
    char buf[2048];
    int found = 0;

    if (f == NULL) {
        return 0;
    }
    print(f, rs);
    rewind(f);
    while (!found && fgets(buf, sizeof(buf), f) != NULL) {
        buf[strcspn(buf, "\n")] = '\0';
        found = json ? strstr(buf, line) != NULL : strcmp(buf, line) == 0;
    }
    fclose(f);
    return found;
}

int main(void)
{
    static char *const argv[] = {"/some/where/bmb_x", "it's", "a\nb\177", "", "-m", "8"};
    bmb_result_set_t rs;
    bmb_result_row_t r;
    char header[256], row[256];
    FILE *f;

    memset(&r, 0, sizeof(r));
    r.thread_count = 1;
    r.dim1 = 8;
    r.time_s = 1.0e-6;
    r.batch = 1;
    r.gbytes_s = 2.5;

    bmb_result_set_init(&rs, "x", "(Size{x} @[y]: ~Z9", NULL, 0, 0, 1);
    bmb_result_set_add(&rs, &r);
    csv_of(&rs, header, row, sizeof(header));
    bmb_result_set_free(&rs);
    check(strcmp(header, "thread_count,size_x_y_z9,time_s,gbytes_per_s,calls_per_sample,median_s,"
                         "q1_s,q3_s") == 0,
          "csv: a label's words joined by one underscore, nothing before the first");
    check(strcmp(row, "1,8,0.000001000,2.500000,1,0.000000000,0.000000000,0.000000000") == 0,
          "csv: no gflops column, and no value for it, without an operation count");
    if (failures != 0) {
        printf("     header \"%s\", row \"%s\"\n", header, row);
    }

    /* The run lines and block (#161). */
    if (setenv("OMP_PROC_BIND", "a b", 1) != 0) {
        check(0, "OMP_PROC_BIND could not be set");
    }
    bmb_result_set_init(&rs, "x", "Size", NULL, 0, 0, 1);
    rs.argc = 6;
    rs.argv = argv;
    rs.warmup = 1;
    rs.iterations = 1;
    rs.batch = 1;
    check(printed(bmb_print_csv, &rs, "# command: bmb_x 'it'\\''s' 'a?b?' '' -m 8", 0),
          "run: the program by its name, a quote, a newline, a DEL and an empty word quoted for a shell");
    check(printed(bmb_print_csv, &rs, "# run: 1 warm-up call, 1 sample, batches of 1 call", 0),
          "run: one warm-up call, one sample, a batch of one, in the singular");
    check(printed(bmb_print_csv, &rs, "OMP_PROC_BIND='a b'", 1),
          "run: a variable set, its value quoted");
    check(printed(bmb_print_json, &rs,
                  "\"command\": [\"bmb_x\", \"it's\", \"a\\u000ab\177\", \"\", \"-m\", \"8\"],", 1),
          "json: the command line word for word, the program by its name");
    check(printed(bmb_print_json, &rs, "\"batch\": 1,", 1), "json: -b, when given");
    check(printed(bmb_print_json, &rs, "\"OMP_PROC_BIND\": \"a b\"", 1),
          "json: a variable set, in the environment");
    rs.warmup = 2;
    rs.iterations = 3;
    rs.batch = 5;
    check(printed(bmb_print_csv, &rs, "# run: 2 warm-up calls, 3 samples, batches of 5 calls", 0),
          "run: several of each, in the plural");
    rs.batch = 0;
    check(printed(bmb_print_csv, &rs, "# run: 2 warm-up calls, 3 samples, batch size chosen per point", 0),
          "run: no -b, the batch size chosen per point");
    check(!printed(bmb_print_json, &rs, "\"batch\"", 1), "json: no batch without -b");
    bmb_result_set_free(&rs);

    /* Reading from a write-only stream sets its error indicator, as a
     * failed write would, without anything left to flush. */
    f = open_device("/dev/null");
    if (f != NULL) {
        /* cppcheck-suppress readWriteOnlyFile */
        (void) fgetc(f);
        check(bmb_print_check_stream(f, "a stream in error") == -1,
              "a stream whose error indicator is set is a failed write, even with nothing to flush");
        fclose(f);
    }

    /* A write still in the buffer, which the flush cannot complete. */
    f = open_device("/dev/full");
    if (f != NULL) {
        fputs("x\n", f);
        check(bmb_print_check_stream(f, "a full disk") == -1, "a flush that fails is a failed write");
        fclose(f);
    } else {
        printf("note no /dev/full here: the failed flush is not checked\n");
    }

    if (failures != 0) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
