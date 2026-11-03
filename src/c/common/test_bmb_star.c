/* Checks which CPUs --star pins its instances to (#173): one per core,
 * the first of each in the affinity, read from sysfs trees generated
 * here; and what it does outside an instance. Running instances is
 * ../test_bmb_star.sh's. */

#define _XOPEN_SOURCE 700

#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bmb_bench.h"
#include "bmb_star.h"

static int failures;

static void check(int ok, const char *what)
{
    printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        failures++;
    }
}

static char tree_root[64];

static void tree_file(const char *rel, const char *content)
{
    char path[512];
    char *p;
    FILE *f;
    int fd;

    snprintf(path, sizeof(path), "%s/%s", tree_root, rel);
    for (p = strchr(path + strlen(tree_root) + 1, '/'); p != NULL; p = strchr(p + 1, '/')) {
        *p = '\0';
        mkdir(path, 0755);
        *p = '/';
    }
    fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    f = (fd >= 0) ? fdopen(fd, "w") : NULL;
    if (f == NULL) {
        check(0, path);
        if (fd >= 0) {
            close(fd);
        }
        return;
    }
    fputs(content, f);
    fclose(f);
}

static int tree_remove_one(const char *path, const struct stat *st, int flag, struct FTW *ftw)
{
    (void) st;
    (void) flag;
    (void) ftw;
    return remove(path);
}

static void tree_begin(void)
{
    snprintf(tree_root, sizeof(tree_root), "bmb_star_XXXXXX");
    if (mkdtemp(tree_root) == NULL) {
        check(0, "a temporary directory for the generated trees");
        exit(1);
    }
}

static void tree_end(void)
{
    nftw(tree_root, tree_remove_one, 16, FTW_DEPTH | FTW_PHYS);
}

/* cpu<c>'s core, as the given file lists it. */
static void core(int c, const char *file, const char *list)
{
    char rel[128];
    char line[64];

    snprintf(rel, sizeof(rel), "sys/devices/system/cpu/cpu%d/topology/%s", c, file);
    snprintf(line, sizeof(line), "%s\n", list);
    tree_file(rel, line);
}

/* The CPUs chosen, as "0,2", against what is wanted. */
static void check_cpus(const char *allowed, int max, const char *want, const char *what)
{
    int cpus[BMB_STAR_MAX];
    char got[256] = "";
    char msg[512];
    int n = bmb_star_cpus_at(tree_root, allowed, cpus, max);
    int i;

    for (i = 0; i < n; i++) {
        snprintf(got + strlen(got), sizeof(got) - strlen(got), "%s%d", i == 0 ? "" : ",", cpus[i]);
    }
    snprintf(msg, sizeof(msg), "%s: got \"%s\", want \"%s\"", what, got, want);
    check(strcmp(got, want) == 0, msg);
}

int main(void)
{
    int cpus[BMB_STAR_MAX];
    int c;

    tree_begin();

    /* No topology: each CPU a core of its own. */
    check_cpus("0-2", BMB_STAR_MAX, "0,1,2", "no topology: every CPU");

    /* Two threads a core, numbered side by side: the first of each. */
    for (c = 0; c < 4; c++) {
        core(c, "core_cpus_list", c < 2 ? "0-1" : "2-3");
    }
    check_cpus("0-3", BMB_STAR_MAX, "0,2", "threads side by side: one a core");
    /* Only the affinity's: the second thread of each core, when the
     * first is not allowed. */
    check_cpus("1,3", BMB_STAR_MAX, "1,3", "an affinity of second threads");
    check_cpus("0-3", 1, "0", "no more than asked for");

    /* Numbered a core apart, as on most x86 servers: 0 and 4 share one. */
    for (c = 0; c < 8; c++) {
        char list[16];

        snprintf(list, sizeof(list), "%d,%d", c % 4, c % 4 + 4);
        core(c, "core_cpus_list", list);
    }
    check_cpus("0-7", BMB_STAR_MAX, "0,1,2,3", "threads a core apart: the first four");
    check_cpus("2-5", BMB_STAR_MAX, "2,3,4,5", "threads a core apart, half of each allowed");

    /* thread_siblings_list, before Linux 5.6, when core_cpus_list is not
     * there; neither, a core of its own. */
    core(8, "thread_siblings_list", "8-9");
    core(9, "thread_siblings_list", "8-9");
    check_cpus("8-10", BMB_STAR_MAX, "8,10", "thread_siblings_list read, a CPU without either alone");

    /* A core list that is not one: the CPU is a core of its own. */
    core(11, "core_cpus_list", "11-x");
    core(12, "core_cpus_list", "11-12");
    check_cpus("11-12", BMB_STAR_MAX, "11", "a malformed core list, the CPU alone");

    /* An affinity that is not a CPU list: none. */
    check_cpus("3-1", BMB_STAR_MAX, "", "a range backwards");
    check_cpus("a", BMB_STAR_MAX, "", "not a number");
    check_cpus("1;2", BMB_STAR_MAX, "", "a separator that is not one");
    check_cpus("", BMB_STAR_MAX, "", "an empty affinity");

    tree_end();

    /* This process's own affinity, on the real sysfs: at least one. */
    check(bmb_star_cpus_at("", NULL, cpus, BMB_STAR_MAX) >= 1, "this process's affinity: a core at least");

    /* The list the results give, consecutive CPUs as a range; what does not
     * fit left out whole. */
    {
        static const struct {
            int cpus[8];
            int count;
            size_t size;
            const char *want;
        } cases[] = {
            {{0, 2, 4}, 3, 64, "0,2,4"},
            {{0, 1, 2, 3}, 4, 64, "0-3"},
            {{0, 1, 2, 3, 8}, 5, 64, "0-3,8"},
            {{0, 1}, 2, 64, "0,1"},
            {{10, 11, 12, 20, 21}, 5, 64, "10-12,20,21"},
            {{5}, 1, 64, "5"},
            {{0}, 0, 64, ""},
            {{0, 1, 2, 3, 8}, 5, 4, "0-3"},
            {{0, 1, 2, 3, 8}, 5, 3, ""},
        };
        size_t k;

        for (k = 0; k < sizeof(cases) / sizeof(cases[0]); k++) {
            char buf[64];
            char what[160];

            memset(buf, 'x', sizeof(buf));
            bmb_star_describe(cases[k].cpus, cases[k].count, buf, cases[k].size);
            snprintf(what, sizeof(what), "the CPUs described in %zu bytes: got \"%s\", want \"%s\"",
                     cases[k].size, buf, cases[k].want);
            check(strcmp(buf, cases[k].want) == 0, what);
        }
        /* No room at all: nothing written, not even the terminator. */
        {
            static const int one[1] = {5};
            char buf[1] = {'x'};

            bmb_star_describe(one, 1, buf, 0);
            check(buf[0] == 'x', "the CPUs described in 0 bytes: nothing written");
        }
    }

    /* Outside --star, nothing to agree with. */
    check(bmb_star_instance() == -1, "outside --star, no instance");
    check(bmb_star_max(2.5) == 2.5, "outside --star, a value is its own largest");
    check(bmb_star_exit_code(0) == EXIT_SUCCESS && bmb_star_exit_code(-1) == EXIT_FAILURE
          && bmb_star_exit_code(-2) == BMB_EXIT_WRONG_RESULT,
          "an instance's status as an exit code");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
