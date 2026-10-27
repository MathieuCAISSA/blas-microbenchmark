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
	single/bmb_sgbmv \
	double/bmb_dgbmv \
	single/bmb_sger \
	double/bmb_dger \
	complex/bmb_cgeru \
	complex16/bmb_zgeru \
	complex/bmb_cgerc \
	complex16/bmb_zgerc \
	single/bmb_ssymv \
	double/bmb_dsymv \
	complex/bmb_chemv \
	complex16/bmb_zhemv \
	single/bmb_sspmv \
	double/bmb_dspmv \
	single/bmb_ssyr \
	double/bmb_dsyr \
	complex/bmb_cher \
	complex16/bmb_zher \
	single/bmb_ssyr2 \
	double/bmb_dsyr2 \
	complex/bmb_cher2 \
	complex16/bmb_zher2 \
	single/bmb_strmv \
	double/bmb_dtrmv \
	complex/bmb_ctrmv \
	complex16/bmb_ztrmv \
	single/bmb_strsv \
	double/bmb_dtrsv \
	complex/bmb_ctrsv \
	complex16/bmb_ztrsv \
	single/bmb_stpsv \
	double/bmb_dtpsv \
	single/bmb_stbsv \
	double/bmb_dtbsv

single_bmb_sgemv_SOURCES = bmb_gemv.c
single_bmb_sgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgemv_SOURCES = bmb_gemv.c
double_bmb_dgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cgemv_SOURCES = bmb_gemv.c
complex_bmb_cgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgemv_SOURCES = bmb_gemv.c
complex16_bmb_zgemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sgbmv_SOURCES = bmb_gbmv.c
single_bmb_sgbmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgbmv_SOURCES = bmb_gbmv.c
double_bmb_dgbmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_sger_SOURCES = bmb_ger.c
single_bmb_sger_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dger_SOURCES = bmb_ger.c
double_bmb_dger_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cgeru_SOURCES = bmb_geru.c
complex_bmb_cgeru_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgeru_SOURCES = bmb_geru.c
complex16_bmb_zgeru_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
complex_bmb_cgerc_SOURCES = bmb_gerc.c
complex_bmb_cgerc_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgerc_SOURCES = bmb_gerc.c
complex16_bmb_zgerc_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssymv_SOURCES = bmb_symv.c
single_bmb_ssymv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsymv_SOURCES = bmb_symv.c
double_bmb_dsymv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_chemv_SOURCES = bmb_hemv.c
complex_bmb_chemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zhemv_SOURCES = bmb_hemv.c
complex16_bmb_zhemv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sspmv_SOURCES = bmb_spmv.c
single_bmb_sspmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dspmv_SOURCES = bmb_spmv.c
double_bmb_dspmv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_ssyr_SOURCES = bmb_syr.c
single_bmb_ssyr_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyr_SOURCES = bmb_syr.c
double_bmb_dsyr_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cher_SOURCES = bmb_her.c
complex_bmb_cher_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zher_SOURCES = bmb_her.c
complex16_bmb_zher_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssyr2_SOURCES = bmb_syr2.c
single_bmb_ssyr2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyr2_SOURCES = bmb_syr2.c
double_bmb_dsyr2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cher2_SOURCES = bmb_her2.c
complex_bmb_cher2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zher2_SOURCES = bmb_her2.c
complex16_bmb_zher2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
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
single_bmb_stpsv_SOURCES = bmb_tpsv.c
single_bmb_stpsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtpsv_SOURCES = bmb_tpsv.c
double_bmb_dtpsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
single_bmb_stbsv_SOURCES = bmb_tbsv.c
single_bmb_stbsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtbsv_SOURCES = bmb_tbsv.c
double_bmb_dtbsv_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D

ROUTINE_TESTS = \
	single/test_bmb_sgemv.sh \
	double/test_bmb_dgemv.sh \
	complex/test_bmb_cgemv.sh \
	complex16/test_bmb_zgemv.sh \
	single/test_bmb_sgbmv.sh \
	double/test_bmb_dgbmv.sh \
	single/test_bmb_sger.sh \
	double/test_bmb_dger.sh \
	complex/test_bmb_cgeru.sh \
	complex16/test_bmb_zgeru.sh \
	complex/test_bmb_cgerc.sh \
	complex16/test_bmb_zgerc.sh \
	single/test_bmb_ssymv.sh \
	double/test_bmb_dsymv.sh \
	complex/test_bmb_chemv.sh \
	complex16/test_bmb_zhemv.sh \
	single/test_bmb_sspmv.sh \
	double/test_bmb_dspmv.sh \
	single/test_bmb_ssyr.sh \
	double/test_bmb_dsyr.sh \
	complex/test_bmb_cher.sh \
	complex16/test_bmb_zher.sh \
	single/test_bmb_ssyr2.sh \
	double/test_bmb_dsyr2.sh \
	complex/test_bmb_cher2.sh \
	complex16/test_bmb_zher2.sh \
	single/test_bmb_strmv.sh \
	double/test_bmb_dtrmv.sh \
	complex/test_bmb_ctrmv.sh \
	complex16/test_bmb_ztrmv.sh \
	single/test_bmb_strsv.sh \
	double/test_bmb_dtrsv.sh \
	complex/test_bmb_ctrsv.sh \
	complex16/test_bmb_ztrsv.sh \
	single/test_bmb_stpsv.sh \
	double/test_bmb_dtpsv.sh \
	single/test_bmb_stbsv.sh \
	double/test_bmb_dtbsv.sh
# END routines (make2)

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = $(ROUTINE_TESTS)

EXTRA_DIST = $(TESTS)

# The source geru and gerc share: each program includes it, the second
# with BMB_CONJ defined (#236); packed storage, which spmv and tpsv read
# (#244), and band storage, which gbmv and tbsv read (#246).
noinst_HEADERS = bmb_geru.h bmb_packed.h bmb_band.h
