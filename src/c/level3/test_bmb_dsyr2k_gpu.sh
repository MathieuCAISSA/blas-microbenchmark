#!/bin/sh
set -e
exec ./bmb_dsyr2k_gpu -x 1 -i 2 -m 8 -M 8
