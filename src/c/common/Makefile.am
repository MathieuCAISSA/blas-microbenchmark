AM_CPPFLAGS = -I$(top_builddir)

noinst_LIBRARIES = libbmbcommon.a

libbmbcommon_a_SOURCES = \
	bmb_log.c bmb_log.h \
	bmb_timer.c bmb_timer.h \
	bmb_options.c bmb_options.h \
	bmb_result.c bmb_result.h \
	bmb_print.c bmb_print.h \
	bmb_threads.c bmb_threads.h \
	bmb_bench.c bmb_bench.h

if HAVE_GPU_CUDA
libbmbcommon_a_SOURCES += bmb_gpu.h bmb_gpu_cuda.c
endif

if HAVE_GPU_ROCM
libbmbcommon_a_SOURCES += bmb_gpu.h bmb_gpu_rocm.c
endif

EXTRA_libbmbcommon_a_SOURCES = bmb_gpu.h bmb_gpu_cuda.c bmb_gpu_rocm.c
