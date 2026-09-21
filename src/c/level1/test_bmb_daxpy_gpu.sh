#!/bin/sh
set -e
exec ./bmb_daxpy_gpu -x 1 -i 2 -v 32
