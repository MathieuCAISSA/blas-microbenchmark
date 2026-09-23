AM_CPPFLAGS = -I$(top_builddir) $(BMB_NETLIB_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)

noinst_LIBRARIES = libbmbcommon.a

libbmbcommon_a_SOURCES = \
	bmb_build.c bmb_build.h \
	bmb_log.c bmb_log.h \
	bmb_timer.c bmb_timer.h \
	bmb_options.c bmb_options.h \
	bmb_result.c bmb_result.h \
	bmb_print.c bmb_print.h \
	bmb_threads.c bmb_threads.h \
	bmb_bench.c bmb_bench.h

# The option layer decides which sizes get measured and rejects the ones
# that cannot be, so its mistakes are silent: a sweep that drops its
# endpoint or a size that truncates to zero still produces a plausible
# table. It gets unit tests of its own, whatever the backend.
check_PROGRAMS = test_bmb_options
test_bmb_options_SOURCES = test_bmb_options.c
test_bmb_options_LDADD = libbmbcommon.a

# Netlib reference BLAS is Fortran-only in the general case, so this
# backend brings its own cblas_* layer forwarding to the Fortran symbols.
if BLAS_BACKEND_NETLIB
libbmbcommon_a_SOURCES += ../netlib/bmb_netlib_cblas.c ../netlib/cblas.h

# That layer has to translate row-major calls to the column-major Fortran
# ABI, which is easy to get silently wrong, so it gets its own correctness
# test on top of the per-benchmark smoke tests.
check_PROGRAMS += test_netlib_cblas
test_netlib_cblas_SOURCES = ../netlib/test_netlib_cblas.c
test_netlib_cblas_LDADD = libbmbcommon.a
endif

TESTS = $(check_PROGRAMS)

# Listed unconditionally so `make dist` ships them whatever this build was
# configured for.
EXTRA_libbmbcommon_a_SOURCES = ../netlib/bmb_netlib_cblas.c ../netlib/cblas.h
EXTRA_DIST = ../netlib/test_netlib_cblas.c
