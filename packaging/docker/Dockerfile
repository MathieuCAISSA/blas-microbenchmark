# blas-microbenchmark in a container, built from this source tree against
# the BLAS that BLAS_BACKEND names: openblas (the default), blis, netlib or
# mkl. From the top of the repository:
#
#     docker build -f packaging/docker/Dockerfile -t blas-microbenchmark .
#     docker build -f packaging/docker/Dockerfile --build-arg BLAS_BACKEND=mkl \
#         -t blas-microbenchmark:mkl .
#     docker run --rm blas-microbenchmark bmb_dgemm -m 64:1024
#
# README.md says how to run it, with Docker or Apptainer, and what a
# container changes in a measurement; doc/dev/packaging.md how it is built
# and tested. FlexiBLAS is left out: Ubuntu has no package of it.

# Both stages start from the same base, pinned by digest as the actions
# are by commit (#13); Dependabot moves both.
FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55 AS build
ARG BLAS_BACKEND=openblas

# The distribution's packages are whatever its archive holds that day: the
# base image is the pin, as on the CI runners.
# hadolint ignore=DL3008,SC2086
RUN case "$BLAS_BACKEND" in \
        openblas) blas=libopenblas-dev ;; \
        blis) blas=libblis-openmp-dev ;; \
        netlib) blas=libblas-dev ;; \
        mkl) blas="ca-certificates curl gpg" ;; \
        *) echo "BLAS_BACKEND is openblas, blis, netlib or mkl, not '$BLAS_BACKEND'" >&2; exit 1 ;; \
    esac \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf automake libtool make gcc libc6-dev pkg-config $blas \
    && rm -rf /var/lib/apt/lists/*

COPY .github/install-oneapi-mkl.sh /tmp/
RUN if [ "$BLAS_BACKEND" = mkl ]; then \
        /tmp/install-oneapi-mkl.sh && rm -rf /var/lib/apt/lists/*; \
    fi

COPY . /src
WORKDIR /src
# A tree configured in place carries its own config.status, Makefiles and
# objects, which would build into the image instead of these sources.
RUN if [ -e config.status ]; then \
        echo "This tree was configured in place: run 'make distclean' in it first." >&2; exit 1; \
    fi \
    && autoreconf -fi

WORKDIR /build
# MKL's libraries are not a package the final image can install, so the
# ones the benchmarks load are copied where their RUNPATH looks: the
# GNU OpenMP layer, the core, and the per-CPU kernels the core opens at
# run time (ldd does not show those), with Intel's licence.
RUN /src/configure --with-blas-backend="$BLAS_BACKEND" \
    && make -j"$(nproc)" \
    && make install DESTDIR=/stage \
    && mkdir -p /stage/usr/local/share/doc/blas-microbenchmark \
    && cp /src/LICENSE /stage/usr/local/share/doc/blas-microbenchmark/ \
    && if [ "$BLAS_BACKEND" = mkl ]; then \
        mkl=$(readlink -f /opt/intel/oneapi/mkl/latest) \
        && mkdir -p /stage/opt/intel/oneapi/mkl/latest/lib /stage/usr/local/share/doc/intel-oneapi-mkl \
        && cp -a "$mkl"/lib/libmkl_core.so* "$mkl"/lib/libmkl_gf_lp64.so* \
            "$mkl"/lib/libmkl_gnu_thread.so* "$mkl"/lib/libmkl_def.so* "$mkl"/lib/libmkl_mc3.so* \
            "$mkl"/lib/libmkl_avx*.so* /stage/opt/intel/oneapi/mkl/latest/lib/ \
        && cp -a "$mkl"/share/doc/mkl/licensing/. /stage/usr/local/share/doc/intel-oneapi-mkl/; \
    fi


FROM ubuntu:24.04@sha256:534baea6a22c03a63003dbc8dbe78fe34bc0d7e595d9a9dc9834884ff530eb55
ARG BLAS_BACKEND=openblas

LABEL org.opencontainers.image.title="blas-microbenchmark" \
      org.opencontainers.image.description="BLAS microbenchmarks, one executable per routine, and bmb_report" \
      org.opencontainers.image.source="https://github.com/MathieuCAISSA/blas-microbenchmark" \
      org.opencontainers.image.licenses="Apache-2.0"

# The minimized image diverts man to a stub that only says so; man-db's
# own is put back. The results go to /results, which belongs to bmb,
# 1001 (ubuntu is 1000): numeric, so that the host can resolve it.
# hadolint ignore=DL3008,SC2086
RUN case "$BLAS_BACKEND" in \
        openblas) blas=libopenblas0-pthread ;; \
        blis) blas=libblis4-openmp ;; \
        netlib) blas=libblas3 ;; \
        mkl) blas=libgomp1 ;; \
    esac \
    && apt-get update \
    && apt-get install -y --no-install-recommends $blas man-db less \
    && rm -rf /var/lib/apt/lists/* \
    && if [ -e /usr/bin/man.REAL ]; then \
        rm /usr/bin/man && dpkg-divert --quiet --remove --rename /usr/bin/man; \
    fi \
    && groupadd --gid 1001 bmb \
    && useradd --uid 1001 --gid 1001 --create-home --shell /bin/bash bmb \
    && mkdir /results \
    && chown 1001:1001 /results

COPY --from=build /stage/ /
ENV PATH=/usr/local/libexec/blas-microbenchmark/level1:/usr/local/libexec/blas-microbenchmark/level2:/usr/local/libexec/blas-microbenchmark/level3:$PATH

# Every library the benchmarks load is here, MKL's kernels included: one
# tiny verified run per level.
RUN bmb_ddot --version \
    && bmb_ddot -x 0 -i 1 -v 64 >/dev/null \
    && bmb_dgemv -x 0 -i 1 -m 32 >/dev/null \
    && bmb_dgemm -x 0 -i 1 -m 32 >/dev/null \
    && bmb_zherk -x 0 -i 1 -m 32 >/dev/null

USER 1001:1001
WORKDIR /results
