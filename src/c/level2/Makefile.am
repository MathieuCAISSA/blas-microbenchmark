AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_CBLAS_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# A directory per precision in the source and build trees; one, level2,
# installed (see level1/Makefile.am).
level2dir = $(pkglibexecdir)/level2

# BEGIN routines (make2): written by src/c/routines.sh from src/c/routines.txt
level2_PROGRAMS = \
	single/bmb_sgemv \
	double/bmb_dgemv \
	double/bmb_dger \
	double/bmb_dsymv \
	double/bmb_dsyr \
	double/bmb_dsyr2 \
	double/bmb_dtrmv \
	double/bmb_dtrsv

single_bmb_sgemv_SOURCES = bmb_gemv.c
single_bmb_sgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgemv_SOURCES = bmb_gemv.c
double_bmb_dgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dger_SOURCES = bmb_ger.c
double_bmb_dger_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dsymv_SOURCES = bmb_symv.c
double_bmb_dsymv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dsyr_SOURCES = bmb_syr.c
double_bmb_dsyr_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dsyr2_SOURCES = bmb_syr2.c
double_bmb_dsyr2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dtrmv_SOURCES = bmb_trmv.c
double_bmb_dtrmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dtrsv_SOURCES = bmb_trsv.c
double_bmb_dtrsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D

ROUTINE_TESTS = \
	single/test_bmb_sgemv.sh \
	double/test_bmb_dgemv.sh \
	double/test_bmb_dger.sh \
	double/test_bmb_dsymv.sh \
	double/test_bmb_dsyr.sh \
	double/test_bmb_dsyr2.sh \
	double/test_bmb_dtrmv.sh \
	double/test_bmb_dtrsv.sh
# END routines (make2)

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = $(ROUTINE_TESTS)

EXTRA_DIST = $(TESTS)
