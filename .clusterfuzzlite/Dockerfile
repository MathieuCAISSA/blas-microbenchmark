# Where ClusterFuzzLite builds the fuzz targets (build.sh), with clang and
# libFuzzer: OSS-Fuzz's builder image, pinned by digest as the actions
# are by commit (#13); Dependabot moves it. configure needs a BLAS to
# find, OpenBLAS as on the CI runners.
FROM gcr.io/oss-fuzz-base/base-builder:v1@sha256:1f9cf74bc3d7029b528494542f9d9851b256dceed38d3f420989434dc5ae0760

# The distribution's packages are whatever its archive holds that day:
# the base image is the pin.
# hadolint ignore=DL3008
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        autoconf automake libtool pkg-config libopenblas-dev zip \
    && rm -rf /var/lib/apt/lists/*

COPY . $SRC/blas-microbenchmark
WORKDIR $SRC/blas-microbenchmark
COPY .clusterfuzzlite/build.sh $SRC/
