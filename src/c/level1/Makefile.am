AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src/c/common $(BMB_CBLAS_CPPFLAGS)
AM_CFLAGS = $(BMB_WARN_CFLAGS)
LDADD = $(top_builddir)/src/c/common/libbmbcommon.a

# Installed to $(libexecdir)/blas-microbenchmark/level1, the way
# osu-micro-benchmarks groups its binaries -- not straight into bin, where
# 32 bmb_* executables would just clutter $PATH. The sources, and so the
# binaries of a build, are in a directory per precision (single, double,
# complex, complex16); installed, they are all in level1, as before
# (#148): a program in a subdirectory is installed by its name.
level1dir = $(pkglibexecdir)/level1

# BEGIN routines (make1): written by src/c/routines.sh from src/c/routines.txt
level1_PROGRAMS = \
	single/bmb_sasum \
	double/bmb_dasum \
	complex/bmb_scasum \
	complex16/bmb_dzasum \
	single/bmb_saxpy \
	double/bmb_daxpy \
	complex/bmb_caxpy \
	complex16/bmb_zaxpy \
	single/bmb_scopy \
	double/bmb_dcopy \
	complex/bmb_ccopy \
	complex16/bmb_zcopy \
	single/bmb_sdot \
	double/bmb_ddot \
	complex/bmb_cdotc \
	complex16/bmb_zdotc \
	single/bmb_snrm2 \
	double/bmb_dnrm2 \
	complex/bmb_scnrm2 \
	complex16/bmb_dznrm2 \
	single/bmb_sscal \
	double/bmb_dscal \
	complex/bmb_cscal \
	complex16/bmb_zscal \
	single/bmb_sswap \
	double/bmb_dswap \
	complex/bmb_cswap \
	complex16/bmb_zswap \
	single/bmb_isamax \
	double/bmb_idamax \
	complex/bmb_icamax \
	complex16/bmb_izamax \
	single/bmb_srot \
	double/bmb_drot

single_bmb_sasum_SOURCES = bmb_asum.c
single_bmb_sasum_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dasum_SOURCES = bmb_asum.c
double_bmb_dasum_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_scasum_SOURCES = bmb_asum.c
complex_bmb_scasum_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_dzasum_SOURCES = bmb_asum.c
complex16_bmb_dzasum_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_saxpy_SOURCES = bmb_axpy.c
single_bmb_saxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_daxpy_SOURCES = bmb_axpy.c
double_bmb_daxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_caxpy_SOURCES = bmb_axpy.c
complex_bmb_caxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zaxpy_SOURCES = bmb_axpy.c
complex16_bmb_zaxpy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_scopy_SOURCES = bmb_copy.c
single_bmb_scopy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dcopy_SOURCES = bmb_copy.c
double_bmb_dcopy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_ccopy_SOURCES = bmb_copy.c
complex_bmb_ccopy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zcopy_SOURCES = bmb_copy.c
complex16_bmb_zcopy_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sdot_SOURCES = bmb_dot.c
single_bmb_sdot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_ddot_SOURCES = bmb_dot.c
double_bmb_ddot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cdotc_SOURCES = bmb_dotc.c
complex_bmb_cdotc_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zdotc_SOURCES = bmb_dotc.c
complex16_bmb_zdotc_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_snrm2_SOURCES = bmb_nrm2.c
single_bmb_snrm2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dnrm2_SOURCES = bmb_nrm2.c
double_bmb_dnrm2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_scnrm2_SOURCES = bmb_nrm2.c
complex_bmb_scnrm2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_dznrm2_SOURCES = bmb_nrm2.c
complex16_bmb_dznrm2_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sscal_SOURCES = bmb_scal.c
single_bmb_sscal_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dscal_SOURCES = bmb_scal.c
double_bmb_dscal_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cscal_SOURCES = bmb_scal.c
complex_bmb_cscal_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zscal_SOURCES = bmb_scal.c
complex16_bmb_zscal_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_sswap_SOURCES = bmb_swap.c
single_bmb_sswap_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_dswap_SOURCES = bmb_swap.c
double_bmb_dswap_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_cswap_SOURCES = bmb_swap.c
complex_bmb_cswap_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_zswap_SOURCES = bmb_swap.c
complex16_bmb_zswap_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_isamax_SOURCES = bmb_iamax.c
single_bmb_isamax_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_idamax_SOURCES = bmb_iamax.c
double_bmb_idamax_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D
complex_bmb_icamax_SOURCES = bmb_iamax.c
complex_bmb_icamax_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_C
complex16_bmb_izamax_SOURCES = bmb_iamax.c
complex16_bmb_izamax_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_Z
single_bmb_srot_SOURCES = bmb_rot.c
single_bmb_srot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_S
double_bmb_drot_SOURCES = bmb_rot.c
double_bmb_drot_CPPFLAGS = $(AM_CPPFLAGS) -DBMB_PREC_D

ROUTINE_TESTS = \
	single/test_bmb_sasum.sh \
	double/test_bmb_dasum.sh \
	complex/test_bmb_scasum.sh \
	complex16/test_bmb_dzasum.sh \
	single/test_bmb_saxpy.sh \
	double/test_bmb_daxpy.sh \
	complex/test_bmb_caxpy.sh \
	complex16/test_bmb_zaxpy.sh \
	single/test_bmb_scopy.sh \
	double/test_bmb_dcopy.sh \
	complex/test_bmb_ccopy.sh \
	complex16/test_bmb_zcopy.sh \
	single/test_bmb_sdot.sh \
	double/test_bmb_ddot.sh \
	complex/test_bmb_cdotc.sh \
	complex16/test_bmb_zdotc.sh \
	single/test_bmb_snrm2.sh \
	double/test_bmb_dnrm2.sh \
	complex/test_bmb_scnrm2.sh \
	complex16/test_bmb_dznrm2.sh \
	single/test_bmb_sscal.sh \
	double/test_bmb_dscal.sh \
	complex/test_bmb_cscal.sh \
	complex16/test_bmb_zscal.sh \
	single/test_bmb_sswap.sh \
	double/test_bmb_dswap.sh \
	complex/test_bmb_cswap.sh \
	complex16/test_bmb_zswap.sh \
	single/test_bmb_isamax.sh \
	double/test_bmb_idamax.sh \
	complex/test_bmb_icamax.sh \
	complex16/test_bmb_izamax.sh \
	single/test_bmb_srot.sh \
	double/test_bmb_drot.sh
# END routines (make1)

# Minimalistic smoke tests (small vector, 1 warmup + 2 timed iterations),
# one per routine, then those of the whole level.
TESTS = $(ROUTINE_TESTS) \
	test_bmb_size_limits.sh

EXTRA_DIST = $(TESTS)
