AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_NETLIB_CPPFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

level3dir = $(pkglibexecdir)/level3

level3_PROGRAMS = \
	bmb_dgemm \
	bmb_dsymm \
	bmb_dsyrk \
	bmb_dsyr2k \
	bmb_dtrmm \
	bmb_dtrsm

bmb_dgemm_SOURCES = bmb_dgemm.c
bmb_dsymm_SOURCES = bmb_dsymm.c
bmb_dsyrk_SOURCES = bmb_dsyrk.c
bmb_dsyr2k_SOURCES = bmb_dsyr2k.c
bmb_dtrmm_SOURCES = bmb_dtrmm.c
bmb_dtrsm_SOURCES = bmb_dtrsm.c

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = \
	test_bmb_dgemm.sh \
	test_bmb_dsymm.sh \
	test_bmb_dsyrk.sh \
	test_bmb_dsyr2k.sh \
	test_bmb_dtrmm.sh \
	test_bmb_dtrsm.sh

EXTRA_DIST = $(TESTS)
