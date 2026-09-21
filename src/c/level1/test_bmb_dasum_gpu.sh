#!/bin/sh
set -e
exec ./bmb_dasum_gpu -x 1 -i 2 -v 32
