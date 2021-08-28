#!/bin/sh
# bld.sh - build the programs on Linux.

LBM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64  # Modify according to your needs.

export LD_LIBRARY_PATH=$LBM/lib

gcc -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l pthread -l lbm -l m \
    -o smx_perf_pub smx_perf_pub.c
if [ $? -ne 0 ]; then exit 1; fi

gcc -I $LBM/include -I $LBM/include/lbm -L $LBM/lib -l pthread -l lbm -l m \
    -o smx_perf_sub smx_perf_sub.c
if [ $? -ne 0 ]; then exit 1; fi
