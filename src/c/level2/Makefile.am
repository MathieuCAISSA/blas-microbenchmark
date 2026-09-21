AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

bin_PROGRAMS = \
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

if HAVE_ANY_GPU
bin_PROGRAMS += \
	bmb_dgemv_gpu \
	bmb_dger_gpu \
	bmb_dsymv_gpu \
	bmb_dsyr_gpu \
	bmb_dsyr2_gpu \
	bmb_dtrmv_gpu \
	bmb_dtrsv_gpu

bmb_dgemv_gpu_SOURCES = bmb_dgemv_gpu.c
bmb_dger_gpu_SOURCES = bmb_dger_gpu.c
bmb_dsymv_gpu_SOURCES = bmb_dsymv_gpu.c
bmb_dsyr_gpu_SOURCES = bmb_dsyr_gpu.c
bmb_dsyr2_gpu_SOURCES = bmb_dsyr2_gpu.c
bmb_dtrmv_gpu_SOURCES = bmb_dtrmv_gpu.c
bmb_dtrsv_gpu_SOURCES = bmb_dtrsv_gpu.c

# Same smoke tests; the *_gpu binaries themselves exit 77 (SKIP) when no
# compatible GPU is present at runtime, so this is safe to run in CI too.
TESTS += \
	test_bmb_dgemv_gpu.sh \
	test_bmb_dger_gpu.sh \
	test_bmb_dsymv_gpu.sh \
	test_bmb_dsyr_gpu.sh \
	test_bmb_dsyr2_gpu.sh \
	test_bmb_dtrmv_gpu.sh \
	test_bmb_dtrsv_gpu.sh
endif

# Listed unconditionally (not just via $(TESTS)) so `make dist` always ships
# every test script, regardless of which backend this build was configured for.
EXTRA_DIST = \
	test_bmb_dgemv.sh test_bmb_dger.sh test_bmb_dsymv.sh test_bmb_dsyr.sh \
	test_bmb_dsyr2.sh test_bmb_dtrmv.sh test_bmb_dtrsv.sh \
	test_bmb_dgemv_gpu.sh test_bmb_dger_gpu.sh test_bmb_dsymv_gpu.sh test_bmb_dsyr_gpu.sh \
	test_bmb_dsyr2_gpu.sh test_bmb_dtrmv_gpu.sh test_bmb_dtrsv_gpu.sh
