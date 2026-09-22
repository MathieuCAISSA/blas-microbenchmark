AM_CPPFLAGS = -I$(top_builddir) $(BMB_NETLIB_CPPFLAGS)

noinst_LIBRARIES = libbmbcommon.a

libbmbcommon_a_SOURCES = \
	bmb_log.c bmb_log.h \
	bmb_timer.c bmb_timer.h \
	bmb_options.c bmb_options.h \
	bmb_result.c bmb_result.h \
	bmb_print.c bmb_print.h \
	bmb_threads.c bmb_threads.h \
	bmb_bench.c bmb_bench.h

# Netlib reference BLAS is Fortran-only in the general case, so this
# backend brings its own cblas_* layer forwarding to the Fortran symbols.
if BLAS_BACKEND_NETLIB
libbmbcommon_a_SOURCES += ../netlib/bmb_netlib_cblas.c ../netlib/cblas.h

# That layer has to translate row-major calls to the column-major Fortran
# ABI, which is easy to get silently wrong, so it gets its own correctness
# test on top of the per-benchmark smoke tests.
check_PROGRAMS = test_netlib_cblas
test_netlib_cblas_SOURCES = ../netlib/test_netlib_cblas.c
test_netlib_cblas_LDADD = libbmbcommon.a
TESTS = $(check_PROGRAMS)
endif

# Listed unconditionally so `make dist` ships them whatever this build was
# configured for.
EXTRA_libbmbcommon_a_SOURCES = ../netlib/bmb_netlib_cblas.c ../netlib/cblas.h
EXTRA_DIST = ../netlib/test_netlib_cblas.c
