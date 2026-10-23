#!/bin/sh
# Installs Intel oneAPI MKL from Intel's apt repository, its current
# release, under /opt/intel/oneapi/mkl: for the mkl CI job, the spack
# job's intel-oneapi-mkl leg, the Docker image (packaging/docker), and a
# Debian or Ubuntu machine to build --with-blas-backend=mkl on. Needs root
# or sudo, curl and gpg; several GB.
#
#     .github/scripts/install-oneapi-mkl.sh
set -e

sudo=sudo
[ "$(id -u)" -ne 0 ] || sudo=
key=/usr/share/keyrings/oneapi-archive-keyring.gpg
curl -fsSL https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS.PUB \
    | gpg --dearmor | $sudo tee "$key" >/dev/null
echo "deb [signed-by=$key] https://apt.repos.intel.com/oneapi all main" \
    | $sudo tee /etc/apt/sources.list.d/oneAPI.list >/dev/null
$sudo apt-get update
$sudo apt-get install -y --no-install-recommends intel-oneapi-mkl-devel
ls -d /opt/intel/oneapi/mkl/[0-9]*
