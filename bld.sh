#!/bin/sh
# bld.sh - build the programs on Linux.

# Set according to your installation.
LBM_PLATFORM=$HOME/UMP_6.14/Linux-glibc-2.17-x86_64

export LD_LIBRARY_PATH=$LBM_PLATFORM/lib

gcc -I $LBM_PLATFORM/include -I $LBM_PLATFORM/include/lbm -L $LBM_PLATFORM/lib -l pthread -l lbm -l m -o smx_perf_pub smx_perf_pub.c
if [ $? -ne 0 ]; then exit 1; fi

gcc -I $LBM_PLATFORM/include -I $LBM_PLATFORM/include/lbm -L $LBM_PLATFORM/lib -l pthread -l lbm -l m -o smx_perf_sub smx_perf_sub.c
if [ $? -ne 0 ]; then exit 1; fi
