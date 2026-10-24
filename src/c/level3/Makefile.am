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

single_bmb_sgemm_SOURCES = single/bmb_sgemm.c
double_bmb_dgemm_SOURCES = double/bmb_dgemm.c
complex_bmb_cgemm_SOURCES = complex/bmb_cgemm.c
complex16_bmb_zgemm_SOURCES = complex16/bmb_zgemm.c
double_bmb_dsymm_SOURCES = double/bmb_dsymm.c
complex16_bmb_zhemm_SOURCES = complex16/bmb_zhemm.c
single_bmb_ssyrk_SOURCES = single/bmb_ssyrk.c
double_bmb_dsyrk_SOURCES = double/bmb_dsyrk.c
complex16_bmb_zherk_SOURCES = complex16/bmb_zherk.c
double_bmb_dsyr2k_SOURCES = double/bmb_dsyr2k.c
double_bmb_dtrmm_SOURCES = double/bmb_dtrmm.c
single_bmb_strsm_SOURCES = single/bmb_strsm.c
double_bmb_dtrsm_SOURCES = double/bmb_dtrsm.c
complex16_bmb_ztrsm_SOURCES = complex16/bmb_ztrsm.c

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
