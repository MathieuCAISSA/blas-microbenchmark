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
	single/bmb_ssymm \
	double/bmb_dsymm \
	complex/bmb_csymm \
	complex16/bmb_zsymm \
	complex/bmb_chemm \
	complex16/bmb_zhemm \
	single/bmb_ssyrk \
	double/bmb_dsyrk \
	complex/bmb_csyrk \
	complex16/bmb_zsyrk \
	complex/bmb_cherk \
	complex16/bmb_zherk \
	single/bmb_ssyr2k \
	double/bmb_dsyr2k \
	complex/bmb_csyr2k \
	complex16/bmb_zsyr2k \
	complex/bmb_cher2k \
	complex16/bmb_zher2k \
	single/bmb_strmm \
	double/bmb_dtrmm \
	complex/bmb_ctrmm \
	complex16/bmb_ztrmm \
	single/bmb_strsm \
	double/bmb_dtrsm \
	complex/bmb_ctrsm \
	complex16/bmb_ztrsm

single_bmb_sgemm_SOURCES = bmb_gemm.c
single_bmb_sgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dgemm_SOURCES = bmb_gemm.c
double_bmb_dgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cgemm_SOURCES = bmb_gemm.c
complex_bmb_cgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zgemm_SOURCES = bmb_gemm.c
complex16_bmb_zgemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssymm_SOURCES = bmb_symm.c
single_bmb_ssymm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsymm_SOURCES = bmb_symm.c
double_bmb_dsymm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_csymm_SOURCES = bmb_symm.c
complex_bmb_csymm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zsymm_SOURCES = bmb_symm.c
complex16_bmb_zsymm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
complex_bmb_chemm_SOURCES = bmb_hemm.c
complex_bmb_chemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zhemm_SOURCES = bmb_hemm.c
complex16_bmb_zhemm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssyrk_SOURCES = bmb_syrk.c
single_bmb_ssyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyrk_SOURCES = bmb_syrk.c
double_bmb_dsyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_csyrk_SOURCES = bmb_syrk.c
complex_bmb_csyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zsyrk_SOURCES = bmb_syrk.c
complex16_bmb_zsyrk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
complex_bmb_cherk_SOURCES = bmb_herk.c
complex_bmb_cherk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zherk_SOURCES = bmb_herk.c
complex16_bmb_zherk_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_ssyr2k_SOURCES = bmb_syr2k.c
single_bmb_ssyr2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dsyr2k_SOURCES = bmb_syr2k.c
double_bmb_dsyr2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_csyr2k_SOURCES = bmb_syr2k.c
complex_bmb_csyr2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zsyr2k_SOURCES = bmb_syr2k.c
complex16_bmb_zsyr2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
complex_bmb_cher2k_SOURCES = bmb_her2k.c
complex_bmb_cher2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zher2k_SOURCES = bmb_her2k.c
complex16_bmb_zher2k_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_strmm_SOURCES = bmb_trmm.c
single_bmb_strmm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtrmm_SOURCES = bmb_trmm.c
double_bmb_dtrmm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_ctrmm_SOURCES = bmb_trmm.c
complex_bmb_ctrmm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_ztrmm_SOURCES = bmb_trmm.c
complex16_bmb_ztrmm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_strsm_SOURCES = bmb_trsm.c
single_bmb_strsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dtrsm_SOURCES = bmb_trsm.c
double_bmb_dtrsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_ctrsm_SOURCES = bmb_trsm.c
complex_bmb_ctrsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_ztrsm_SOURCES = bmb_trsm.c
complex16_bmb_ztrsm_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z

ROUTINE_TESTS = \
	single/test_bmb_sgemm.sh \
	double/test_bmb_dgemm.sh \
	complex/test_bmb_cgemm.sh \
	complex16/test_bmb_zgemm.sh \
	single/test_bmb_ssymm.sh \
	double/test_bmb_dsymm.sh \
	complex/test_bmb_csymm.sh \
	complex16/test_bmb_zsymm.sh \
	complex/test_bmb_chemm.sh \
	complex16/test_bmb_zhemm.sh \
	single/test_bmb_ssyrk.sh \
	double/test_bmb_dsyrk.sh \
	complex/test_bmb_csyrk.sh \
	complex16/test_bmb_zsyrk.sh \
	complex/test_bmb_cherk.sh \
	complex16/test_bmb_zherk.sh \
	single/test_bmb_ssyr2k.sh \
	double/test_bmb_dsyr2k.sh \
	complex/test_bmb_csyr2k.sh \
	complex16/test_bmb_zsyr2k.sh \
	complex/test_bmb_cher2k.sh \
	complex16/test_bmb_zher2k.sh \
	single/test_bmb_strmm.sh \
	double/test_bmb_dtrmm.sh \
	complex/test_bmb_ctrmm.sh \
	complex16/test_bmb_ztrmm.sh \
	single/test_bmb_strsm.sh \
	double/test_bmb_dtrsm.sh \
	complex/test_bmb_ctrsm.sh \
	complex16/test_bmb_ztrsm.sh
# END routines (make3)

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations),
# one per routine, then those of the whole level.
TESTS = $(ROUTINE_TESTS) \
	test_bmb_square_default.sh

EXTRA_DIST = $(TESTS)

# The sources symm and hemm, syrk and herk, syr2k and her2k share: each
# pair of programs includes one, the second with BMB_HERM defined (#209,
# #242).
noinst_HEADERS = bmb_symm.h bmb_syrk.h bmb_syr2k.h
