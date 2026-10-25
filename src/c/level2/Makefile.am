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
	complex/bmb_cgemv \
	complex16/bmb_zgemv \
	single/bmb_sger \
	double/bmb_dger \
	single/bmb_ssymv \
	double/bmb_dsymv \
	single/bmb_ssyr \
	double/bmb_dsyr \
	single/bmb_ssyr2 \
	double/bmb_dsyr2 \
	single/bmb_strmv \
	double/bmb_dtrmv \
	complex/bmb_ctrmv \
	complex16/bmb_ztrmv \
	single/bmb_strsv \
	double/bmb_dtrsv \
	complex/bmb_ctrsv \
	complex16/bmb_ztrsv

single_bmb_sgemv_SOURCES = bmb_gemv.c
single_bmb_sgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgemv_SOURCES = bmb_gemv.c
double_bmb_dgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cgemv_SOURCES = bmb_gemv.c
complex_bmb_cgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgemv_SOURCES = bmb_gemv.c
complex16_bmb_zgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sger_SOURCES = bmb_ger.c
single_bmb_sger_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dger_SOURCES = bmb_ger.c
double_bmb_dger_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_ssymv_SOURCES = bmb_symv.c
single_bmb_ssymv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsymv_SOURCES = bmb_symv.c
double_bmb_dsymv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_ssyr_SOURCES = bmb_syr.c
single_bmb_ssyr_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyr_SOURCES = bmb_syr.c
double_bmb_dsyr_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_ssyr2_SOURCES = bmb_syr2.c
single_bmb_ssyr2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyr2_SOURCES = bmb_syr2.c
double_bmb_dsyr2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_strmv_SOURCES = bmb_trmv.c
single_bmb_strmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtrmv_SOURCES = bmb_trmv.c
double_bmb_dtrmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_ctrmv_SOURCES = bmb_trmv.c
complex_bmb_ctrmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_ztrmv_SOURCES = bmb_trmv.c
complex16_bmb_ztrmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_strsv_SOURCES = bmb_trsv.c
single_bmb_strsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtrsv_SOURCES = bmb_trsv.c
double_bmb_dtrsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_ctrsv_SOURCES = bmb_trsv.c
complex_bmb_ctrsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_ztrsv_SOURCES = bmb_trsv.c
complex16_bmb_ztrsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z

ROUTINE_TESTS = \
	single/test_bmb_sgemv.sh \
	double/test_bmb_dgemv.sh \
	complex/test_bmb_cgemv.sh \
	complex16/test_bmb_zgemv.sh \
	single/test_bmb_sger.sh \
	double/test_bmb_dger.sh \
	single/test_bmb_ssymv.sh \
	double/test_bmb_dsymv.sh \
	single/test_bmb_ssyr.sh \
	double/test_bmb_dsyr.sh \
	single/test_bmb_ssyr2.sh \
	double/test_bmb_dsyr2.sh \
	single/test_bmb_strmv.sh \
	double/test_bmb_dtrmv.sh \
	complex/test_bmb_ctrmv.sh \
	complex16/test_bmb_ztrmv.sh \
	single/test_bmb_strsv.sh \
	double/test_bmb_dtrsv.sh \
	complex/test_bmb_ctrsv.sh \
	complex16/test_bmb_ztrsv.sh
# END routines (make2)

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = $(ROUTINE_TESTS)

EXTRA_DIST = $(TESTS)
