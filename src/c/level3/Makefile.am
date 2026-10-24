AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_CBLAS_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# A directory per precision in the source and build trees; one, level3,
# installed (see level1/Makefile.am).
level3dir = $(pkglibexecdir)/level3

# BEGIN routines (make3): written by src/c/routines.sh from src/c/routines.txt
level3_PROGRAMS = \
	single/bmb_sgemm \
	double/bmb_dgemm \
	complex/bmb_cgemm \
	complex16/bmb_zgemm \
	double/bmb_dsymm \
	complex16/bmb_zhemm \
	single/bmb_ssyrk \
	double/bmb_dsyrk \
	complex16/bmb_zherk \
	double/bmb_dsyr2k \
	double/bmb_dtrmm \
	single/bmb_strsm \
	double/bmb_dtrsm \
	complex16/bmb_ztrsm

single_bmb_sgemm_SOURCES = bmb_gemm.c
single_bmb_sgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgemm_SOURCES = bmb_gemm.c
double_bmb_dgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cgemm_SOURCES = bmb_gemm.c
complex_bmb_cgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgemm_SOURCES = bmb_gemm.c
complex16_bmb_zgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
double_bmb_dsymm_SOURCES = bmb_symm.c
double_bmb_dsymm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex16_bmb_zhemm_SOURCES = bmb_hemm.c
complex16_bmb_zhemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssyrk_SOURCES = bmb_syrk.c
single_bmb_ssyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyrk_SOURCES = bmb_syrk.c
double_bmb_dsyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex16_bmb_zherk_SOURCES = bmb_herk.c
complex16_bmb_zherk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
double_bmb_dsyr2k_SOURCES = bmb_syr2k.c
double_bmb_dsyr2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
double_bmb_dtrmm_SOURCES = bmb_trmm.c
double_bmb_dtrmm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_strsm_SOURCES = bmb_trsm.c
single_bmb_strsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtrsm_SOURCES = bmb_trsm.c
double_bmb_dtrsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex16_bmb_ztrsm_SOURCES = bmb_trsm.c
complex16_bmb_ztrsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z

ROUTINE_TESTS = \
	single/test_bmb_sgemm.sh \
	double/test_bmb_dgemm.sh \
	complex/test_bmb_cgemm.sh \
	complex16/test_bmb_zgemm.sh \
	double/test_bmb_dsymm.sh \
	complex16/test_bmb_zhemm.sh \
	single/test_bmb_ssyrk.sh \
	double/test_bmb_dsyrk.sh \
	complex16/test_bmb_zherk.sh \
	double/test_bmb_dsyr2k.sh \
	double/test_bmb_dtrmm.sh \
	single/test_bmb_strsm.sh \
	double/test_bmb_dtrsm.sh \
	complex16/test_bmb_ztrsm.sh
# END routines (make3)

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations),
# one per routine, then those of the whole level.
TESTS = $(ROUTINE_TESTS) \
	test_bmb_square_default.sh

EXTRA_DIST = $(TESTS)

# The sources symm and hemm, syrk and herk share: each pair of programs
# includes one, the second with BMB_HERM defined (#209).
noinst_HEADERS = bmb_symm.h bmb_syrk.h
