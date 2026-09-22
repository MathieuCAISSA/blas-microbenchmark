AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_NETLIB_CPPFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

level2dir = $(pkglibexecdir)/level2

level2_PROGRAMS = \
	bmb_dgemv \
	bmb_dger \
	bmb_dsymv \
	bmb_dsyr \
	bmb_dsyr2 \
	bmb_dtrmv \
	bmb_dtrsv

bmb_dgemv_SOURCES = bmb_dgemv.c
bmb_dger_SOURCES = bmb_dger.c
bmb_dsymv_SOURCES = bmb_dsymv.c
bmb_dsyr_SOURCES = bmb_dsyr.c
bmb_dsyr2_SOURCES = bmb_dsyr2.c
bmb_dtrmv_SOURCES = bmb_dtrmv.c
bmb_dtrsv_SOURCES = bmb_dtrsv.c

# Minimalistic smoke tests (small matrices, 1 warmup + 2 timed iterations).
TESTS = \
	test_bmb_dgemv.sh \
	test_bmb_dger.sh \
	test_bmb_dsymv.sh \
	test_bmb_dsyr.sh \
	test_bmb_dsyr2.sh \
	test_bmb_dtrmv.sh \
	test_bmb_dtrsv.sh

EXTRA_DIST = $(TESTS)
