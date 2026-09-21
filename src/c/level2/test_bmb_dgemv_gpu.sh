#!/bin/sh
set -e
exec ./bmb_dgemv_gpu -x 1 -i 2 -m 8 -M 8
