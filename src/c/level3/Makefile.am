AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_CBLAS_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# A directory per precision in the source and build trees; one, level3,
# installed (see level1/Makefile.am).
level3dir = $(pkglibexecdir)/level3

level3_PROGRAMS = \
	single/bmb_sgemm \
	single/bmb_strsm \
	single/bmb_ssyrk \
	double/bmb_dgemm \
	double/bmb_dsymm \
	double/bmb_dsyrk \
	double/bmb_dsyr2k \
	double/bmb_dtrmm \
	double/bmb_dtrsm \
	complex/bmb_cgemm \
	complex16/bmb_zgemm \
	complex16/bmb_ztrsm \
	complex16/bmb_zherk \
	complex16/bmb_zhemm

single_bmb_sgemm_SOURCES = single/bmb_sgemm.c
single_bmb_strsm_SOURCES = single/bmb_strsm.c
single_bmb_ssyrk_SOURCES = single/bmb_ssyrk.c
double_bmb_dgemm_SOURCES = double/bmb_dgemm.c
double_bmb_dsymm_SOURCES = double/bmb_dsymm.c
double_bmb_dsyrk_SOURCES = double/bmb_dsyrk.c
double_bmb_dsyr2k_SOURCES = double/bmb_dsyr2k.c
double_bmb_dtrmm_SOURCES = double/bmb_dtrmm.c
double_bmb_dtrsm_SOURCES = double/bmb_dtrsm.c
complex_bmb_cgemm_SOURCES = complex/bmb_cgemm.c
complex16_bmb_zgemm_SOURCES = complex16/bmb_zgemm.c
complex16_bmb_ztrsm_SOURCES = complex16/bmb_ztrsm.c
complex16_bmb_zherk_SOURCES = complex16/bmb_zherk.c
complex16_bmb_zhemm_SOURCES = complex16/bmb_zhemm.c

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = \
	single/test_bmb_sgemm.sh \
	single/test_bmb_strsm.sh \
	single/test_bmb_ssyrk.sh \
	double/test_bmb_dgemm.sh \
	double/test_bmb_dsymm.sh \
	double/test_bmb_dsyrk.sh \
	double/test_bmb_dsyr2k.sh \
	double/test_bmb_dtrmm.sh \
	double/test_bmb_dtrsm.sh \
	complex/test_bmb_cgemm.sh \
	complex16/test_bmb_zgemm.sh \
	complex16/test_bmb_ztrsm.sh \
	complex16/test_bmb_zherk.sh \
	complex16/test_bmb_zhemm.sh \
	test_bmb_output_formats.sh \
	test_bmb_square_default.sh

EXTRA_DIST = $(TESTS)
