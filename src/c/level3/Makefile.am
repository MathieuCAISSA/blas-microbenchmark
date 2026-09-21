AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

bin_PROGRAMS = \
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

if HAVE_ANY_GPU
bin_PROGRAMS += \
	bmb_dgemm_gpu \
	bmb_dsymm_gpu \
	bmb_dsyrk_gpu \
	bmb_dsyr2k_gpu \
	bmb_dtrmm_gpu \
	bmb_dtrsm_gpu

bmb_dgemm_gpu_SOURCES = bmb_dgemm_gpu.c
bmb_dsymm_gpu_SOURCES = bmb_dsymm_gpu.c
bmb_dsyrk_gpu_SOURCES = bmb_dsyrk_gpu.c
bmb_dsyr2k_gpu_SOURCES = bmb_dsyr2k_gpu.c
bmb_dtrmm_gpu_SOURCES = bmb_dtrmm_gpu.c
bmb_dtrsm_gpu_SOURCES = bmb_dtrsm_gpu.c

# Same smoke tests; the *_gpu binaries themselves exit 77 (SKIP) when no
# compatible GPU is present at runtime, so this is safe to run in CI too.
TESTS += \
	test_bmb_dgemm_gpu.sh \
	test_bmb_dsymm_gpu.sh \
	test_bmb_dsyrk_gpu.sh \
	test_bmb_dsyr2k_gpu.sh \
	test_bmb_dtrmm_gpu.sh \
	test_bmb_dtrsm_gpu.sh
endif

# Listed unconditionally (not just via $(TESTS)) so `make dist` always ships
# every test script, regardless of which backend this build was configured for.
EXTRA_DIST = \
	test_bmb_dgemm.sh test_bmb_dsymm.sh test_bmb_dsyrk.sh test_bmb_dsyr2k.sh \
	test_bmb_dtrmm.sh test_bmb_dtrsm.sh \
	test_bmb_dgemm_gpu.sh test_bmb_dsymm_gpu.sh test_bmb_dsyrk_gpu.sh test_bmb_dsyr2k_gpu.sh \
	test_bmb_dtrmm_gpu.sh test_bmb_dtrsm_gpu.sh
