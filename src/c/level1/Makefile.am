AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

bin_PROGRAMS = \
	bmb_dasum \
	bmb_daxpy \
	bmb_dcopy \
	bmb_ddot \
	bmb_dnrm2 \
	bmb_dscal \
	bmb_dswap

bmb_dasum_SOURCES = bmb_dasum.c
bmb_daxpy_SOURCES = bmb_daxpy.c
bmb_dcopy_SOURCES = bmb_dcopy.c
bmb_ddot_SOURCES = bmb_ddot.c
bmb_dnrm2_SOURCES = bmb_dnrm2.c
bmb_dscal_SOURCES = bmb_dscal.c
bmb_dswap_SOURCES = bmb_dswap.c

# Minimalistic smoke tests (small vector, 1 warmup + 2 timed iterations).
TESTS = \
	test_bmb_dasum.sh \
	test_bmb_daxpy.sh \
	test_bmb_dcopy.sh \
	test_bmb_ddot.sh \
	test_bmb_dnrm2.sh \
	test_bmb_dscal.sh \
	test_bmb_dswap.sh

if HAVE_ANY_GPU
bin_PROGRAMS += \
	bmb_dasum_gpu \
	bmb_daxpy_gpu \
	bmb_dcopy_gpu \
	bmb_ddot_gpu \
	bmb_dnrm2_gpu \
	bmb_dscal_gpu \
	bmb_dswap_gpu

bmb_dasum_gpu_SOURCES = bmb_dasum_gpu.c
bmb_daxpy_gpu_SOURCES = bmb_daxpy_gpu.c
bmb_dcopy_gpu_SOURCES = bmb_dcopy_gpu.c
bmb_ddot_gpu_SOURCES = bmb_ddot_gpu.c
bmb_dnrm2_gpu_SOURCES = bmb_dnrm2_gpu.c
bmb_dscal_gpu_SOURCES = bmb_dscal_gpu.c
bmb_dswap_gpu_SOURCES = bmb_dswap_gpu.c

# Same smoke tests; the *_gpu binaries themselves exit 77 (SKIP) when no
# compatible GPU is present at runtime, so this is safe to run in CI too.
TESTS += \
	test_bmb_dasum_gpu.sh \
	test_bmb_daxpy_gpu.sh \
	test_bmb_dcopy_gpu.sh \
	test_bmb_ddot_gpu.sh \
	test_bmb_dnrm2_gpu.sh \
	test_bmb_dscal_gpu.sh \
	test_bmb_dswap_gpu.sh
endif

# Listed unconditionally (not just via $(TESTS)) so `make dist` always ships
# every test script, regardless of which backend this build was configured for.
EXTRA_DIST = \
	test_bmb_dasum.sh test_bmb_daxpy.sh test_bmb_dcopy.sh test_bmb_ddot.sh \
	test_bmb_dnrm2.sh test_bmb_dscal.sh test_bmb_dswap.sh \
	test_bmb_dasum_gpu.sh test_bmb_daxpy_gpu.sh test_bmb_dcopy_gpu.sh test_bmb_ddot_gpu.sh \
	test_bmb_dnrm2_gpu.sh test_bmb_dscal_gpu.sh test_bmb_dswap_gpu.sh
