/* A test helper, never installed: loaded with LD_PRELOAD by
 * test_bmb_alloc_failures.sh, it makes the BMB_FAIL_ALLOC-th allocation
 * the program's own code asks for (malloc, calloc, realloc; counting from
 * 1) return NULL, as an exhausted heap would. With BMB_ALLOC_COUNT set,
 * it says on stderr how many there were ("bmb_failalloc: N allocations"),
 * so that the test knows how many to try.
 *
 * Only the program's allocations count: those whose caller is in its own
 * executable segment, not the C library's (fopen, strdup) nor the BLAS
 * library's, whose failures are theirs to handle. And only while main
 * runs: the coverage runtime linked into the program allocates at exit,
 * and failing it there would corrupt the coverage data, not test the
 * benchmark. glibc only (__libc_malloc, __libc_start_main). */
#define _GNU_SOURCE

#include <dlfcn.h>
#include <errno.h>
#include <link.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

extern void *__libc_malloc(size_t size);
extern void *__libc_calloc(size_t count, size_t size);
extern void *__libc_realloc(void *ptr, size_t size);

typedef int (*main_fn)(int, char **, char **);
typedef int (*start_main_fn)(main_fn, int, char **, void (*)(void), void (*)(void),
                             void (*)(void), void *);

static uintptr_t text_lo, text_hi;
static long fail_at;
static long counted;
static int in_main;
static main_fn real_main;

/* The first object dl_iterate_phdr gives is the program itself. */
static int find_text(struct dl_phdr_info *info, size_t size, void *data)
{
    int i;

    (void) size;
    (void) data;
    for (i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *ph = &info->dlpi_phdr[i];

        if (ph->p_type == PT_LOAD && (ph->p_flags & PF_X)) {
            text_lo = info->dlpi_addr + ph->p_vaddr;
            text_hi = text_lo + ph->p_memsz;
        }
    }
    return 1;
}

static int fails(const void *caller)
{
    uintptr_t at = (uintptr_t) caller;

    if (!in_main || at < text_lo || at >= text_hi) {
        return 0;
    }
    counted++;
    if (counted == fail_at) {
        errno = ENOMEM;
        return 1;
    }
    return 0;
}

void *malloc(size_t size)
{
    return fails(__builtin_return_address(0)) ? NULL : __libc_malloc(size);
}

void *calloc(size_t count, size_t size)
{
    return fails(__builtin_return_address(0)) ? NULL : __libc_calloc(count, size);
}

void *realloc(void *ptr, size_t size)
{
    return fails(__builtin_return_address(0)) ? NULL : __libc_realloc(ptr, size);
}

static int wrapped_main(int argc, char **argv, char **envp)
{
    const char *at = getenv("BMB_FAIL_ALLOC");
    int report = getenv("BMB_ALLOC_COUNT") != NULL;
    int status;

    fail_at = (at != NULL) ? strtol(at, NULL, 10) : 0;
    dl_iterate_phdr(find_text, NULL);
    in_main = 1;
    status = real_main(argc, argv, envp);
    in_main = 0;
    if (report) {
        fprintf(stderr, "bmb_failalloc: %ld allocations\n", counted);
    }
    return status;
}

int __libc_start_main(main_fn main, int argc, char **argv, void (*init)(void),
                      void (*fini)(void), void (*rtld_fini)(void), void *stack_end)
{
    start_main_fn real = (start_main_fn) dlsym(RTLD_NEXT, "__libc_start_main");

    real_main = main;
    return real(wrapped_main, argc, argv, init, fini, rtld_fini, stack_end);
}
