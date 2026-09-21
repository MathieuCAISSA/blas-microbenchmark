#!/bin/sh
set -e
exec ./bmb_dcopy_gpu -x 1 -i 2 -v 32
