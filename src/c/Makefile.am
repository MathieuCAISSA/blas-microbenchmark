SUBDIRS = common level1 level2 level3

# Runs every benchmark of level{1,2,3} with --verify, so it lives above
# them: "." is built and checked after SUBDIRS.
TESTS = test_bmb_verify.sh test_bmb_threads_env.sh test_bmb_batch.sh \
	test_bmb_ignored_options.sh test_bmb_write_errors.sh \
	test_bmb_alloc_failures.sh test_bmb_colors.sh test_bmb_cache.sh \
	test_bmb_levels.sh test_bmb_kernels.sh test_bmb_output_formats.sh \
	test_bmb_counts.sh
# test_bmb_threads_env.sh reads config.h for the backend's thread control.
AM_TESTS_ENVIRONMENT = top_builddir='$(top_builddir)'; export top_builddir;

# test_helper.sh is sourced by every per-routine smoke test in level{1,2,3}.
# routines.txt is the table of routines, routines.sh what writes their
# lists from it (tests/test_routines.sh checks them).
EXTRA_DIST = test_helper.sh bmb_failalloc.c routines.txt routines.sh $(TESTS)

# The fault-injection library test_bmb_alloc_failures.sh preloads: built
# for make check only and never installed, and without the build's CFLAGS,
# so that neither coverage nor a sanitizer instruments what does the
# testing.
check_DATA = libbmb_failalloc.so
CLEANFILES = libbmb_failalloc.so
libbmb_failalloc.so: $(srcdir)/bmb_failalloc.c
	$(AM_V_CC)$(CC) $(BMB_WARN_CFLAGS) -O2 -fPIC -shared -o $@ $(srcdir)/bmb_failalloc.c -ldl
