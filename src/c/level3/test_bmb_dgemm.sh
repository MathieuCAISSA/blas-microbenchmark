#!/bin/sh
set -e
exec ./bmb_dgemm -x 1 -i 2 -m 8 -M 8
