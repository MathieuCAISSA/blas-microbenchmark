#!/bin/sh
set -e
exec ./bmb_dtrsv_gpu -x 1 -i 2 -m 8
