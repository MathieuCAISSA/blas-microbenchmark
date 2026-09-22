AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_NETLIB_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# Installed to $(libexecdir)/blas-microbenchmark/level1, the way
# osu-micro-benchmarks groups its binaries -- not straight into bin, where
# 20 bmb_* executables would just clutter $PATH.
level1dir = $(pkglibexecdir)/level1

level1_PROGRAMS = \
	bmb_dasum \
	bmb_daxpy \
	bmb_dcopy \
	bmb_ddot \
	bmb_dnrm2 \
	bmb_dscal \
	bmb_dswap

bmb_dasum_SOURCES = bmb_dasum.c
bmb_daxpy_SOURCES = bmb_daxpy.c
bmb_dcopy_SOURCES = bmb_dcopy.c
bmb_ddot_SOURCES = bmb_ddot.c
bmb_dnrm2_SOURCES = bmb_dnrm2.c
bmb_dscal_SOURCES = bmb_dscal.c
bmb_dswap_SOURCES = bmb_dswap.c

# Minimalistic smoke tests (small vector, 1 warmup + 2 timed iterations).
TESTS = \
	test_bmb_dasum.sh \
	test_bmb_daxpy.sh \
	test_bmb_dcopy.sh \
	test_bmb_ddot.sh \
	test_bmb_dnrm2.sh \
	test_bmb_dscal.sh \
	test_bmb_dswap.sh

EXTRA_DIST = $(TESTS)
