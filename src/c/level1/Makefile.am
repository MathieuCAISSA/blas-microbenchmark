AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_CBLAS_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# Installed to $(libexecdir)/blas-microbenchmark/level1, the way
# osu-micro-benchmarks groups its binaries -- not straight into bin, where
# 32 bmb_* executables would just clutter $PATH. The sources, and so the
# binaries of a build, are in a directory per precision (single, double,
# complex, complex16); installed, they are all in level1, as before
# (#148): a program in a subdirectory is installed by its name.
level1dir = $(pkglibexecdir)/level1

# BEGIN routines (make1): written by src/c/routines.sh from src/c/routines.txt
level1_PROGRAMS = \
	double/bmb_dasum \
	single/bmb_saxpy \
	double/bmb_daxpy \
	double/bmb_dcopy \
	single/bmb_sdot \
	double/bmb_ddot \
	complex16/bmb_zdotc \
	double/bmb_dnrm2 \
	double/bmb_dscal \
	double/bmb_dswap

double_bmb_dasum_SOURCES = bmb_asum.c
double_bmb_dasum_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_saxpy_SOURCES = bmb_axpy.c
single_bmb_saxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_daxpy_SOURCES = bmb_axpy.c
double_bmb_daxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dcopy_SOURCES = bmb_copy.c
double_bmb_dcopy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_sdot_SOURCES = bmb_dot.c
single_bmb_sdot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_ddot_SOURCES = bmb_dot.c
double_bmb_ddot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex16_bmb_zdotc_SOURCES = bmb_dotc.c
complex16_bmb_zdotc_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
double_bmb_dnrm2_SOURCES = bmb_nrm2.c
double_bmb_dnrm2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dscal_SOURCES = bmb_scal.c
double_bmb_dscal_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dswap_SOURCES = bmb_swap.c
double_bmb_dswap_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D

ROUTINE_TESTS = \
	double/test_bmb_dasum.sh \
	single/test_bmb_saxpy.sh \
	double/test_bmb_daxpy.sh \
	double/test_bmb_dcopy.sh \
	single/test_bmb_sdot.sh \
	double/test_bmb_ddot.sh \
	complex16/test_bmb_zdotc.sh \
	double/test_bmb_dnrm2.sh \
	double/test_bmb_dscal.sh \
	double/test_bmb_dswap.sh
# END routines (make1)

# Minimalistic smoke tests (small vector, 1 warmup + 2 timed iterations),
# one per routine, then those of the whole level.
TESTS = $(ROUTINE_TESTS) \
	test_bmb_size_limits.sh

EXTRA_DIST = $(TESTS)
